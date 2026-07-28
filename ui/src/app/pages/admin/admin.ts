import { Component, DestroyRef, computed, inject, OnInit, signal } from '@angular/core';
import { FormControl, ReactiveFormsModule } from '@angular/forms';
import { ActivatedRoute, NavigationEnd, Router, RouterLink, RouterOutlet } from '@angular/router';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { filter, take } from 'rxjs';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatSelectModule } from '@angular/material/select';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { DatabaseSchema } from '@honuware/ui/access';
import { DatabaseSchemaService } from '@honuware/ui/crud';

// "Manage Data" — the table-picker shell (mounted at /admin/tables): a root-table
// picker over the live get_db_schema, with the selected table's CRUD editor
// (@honuware/ui/crud's TableView/Edit/New pages) rendering in the outlet below. The
// library ships the per-table editor pages but no shell, so this is CommunityFinder's.
// Its child routes sit directly under 'tables', matching the library's default
// basePath '/admin/tables'. The dashboard landing is a sibling (AdminDashboard, /admin).
@Component({
  selector: 'app-admin',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    RouterOutlet,
    MatFormFieldModule,
    MatSelectModule,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
  ],
  templateUrl: './admin.html',
  styleUrl: './admin.scss',
})
export class AdminPage implements OnInit {
  private schemaService = inject(DatabaseSchemaService);
  private router = inject(Router);
  private route = inject(ActivatedRoute);
  private destroyRef = inject(DestroyRef);

  readonly schema = signal<DatabaseSchema>({
    root_tables: [],
    nested_tables: [],
    tables: [],
    display_templates: {},
    fk_picker_preload_threshold: 50,
  });
  readonly loading = signal(false);
  readonly tableControl = new FormControl<string>('', { nonNullable: true });

  // Tables shown in the picker: the top-level tables minus the nested/junction tables
  // (role_assignments, role_permissions). Those are edited from their parent (roles /
  // people) or a bespoke page — their generic New form is empty anyway, since the
  // framework marks their FK columns readonly.
  readonly pickerTables = computed(() => {
    const schema = this.schema();
    const nested = new Set(schema.nested_tables);
    return schema.root_tables.filter((table) => !nested.has(table));
  });

  ngOnInit(): void {
    this.loadSchema();
    this.syncSelectionFromRoute();
    // Keep the picker in sync when navigation lands on a table (deep link / back).
    this.router.events
      .pipe(
        filter((event) => event instanceof NavigationEnd),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe(() => this.syncSelectionFromRoute());
    // Picking a table opens its CRUD view page.
    this.tableControl.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((tableName) => {
        if (tableName) {
          this.router.navigate([tableName, 'view', '10', '0'], {
            relativeTo: this.route,
          });
        }
      });
  }

  // The friendly label for a table name — from the schema when present, else a
  // title-cased fallback (mirrors the framework's admin_table_friendly_names).
  friendlyName(tableName: string): string {
    const table = this.schema().tables.find((t) => t.table_name === tableName);
    if (table?.table_friendly_name) {
      return table.table_friendly_name;
    }
    const spaced = tableName.replace(/_/g, ' ');
    return spaced.charAt(0).toUpperCase() + spaced.slice(1);
  }

  refresh(): void {
    this.schemaService.refreshSchema();
    this.loadSchema();
  }

  private loadSchema(): void {
    this.loading.set(true);
    this.tableControl.disable({ emitEvent: false });
    this.schemaService
      .GetDBSchema()
      .pipe(take(1), takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (schema) => {
          this.schema.set(schema);
          this.loading.set(false);
          this.tableControl.enable({ emitEvent: false });
          // Landing on the bare /admin/tables route: open the first picker table.
          const firstTable = this.pickerTables()[0];
          if (!this.route.firstChild && firstTable) {
            this.router.navigate([firstTable, 'view', '10', '0'], {
              relativeTo: this.route,
            });
          }
        },
        error: () => {
          this.loading.set(false);
          this.tableControl.enable({ emitEvent: false });
        },
      });
  }

  private syncSelectionFromRoute(): void {
    const tableName = this.route.firstChild?.snapshot.paramMap.get('tableName') ?? '';
    this.tableControl.setValue(tableName, { emitEvent: false });
  }
}

import { Component, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { RouterLink } from '@angular/router';
import { FormControl, ReactiveFormsModule } from '@angular/forms';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import {
  debounceTime,
  distinctUntilChanged,
  filter,
  map,
  of,
  switchMap,
  tap,
} from 'rxjs';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatSelectModule } from '@angular/material/select';
import { MatInputModule } from '@angular/material/input';
import {
  MatAutocompleteModule,
  MatAutocompleteSelectedEvent,
} from '@angular/material/autocomplete';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatListModule } from '@angular/material/list';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import {
  DataResultsWithCount,
  FkOption,
  FkOptionsResponse,
  HONUWARE_CRUD_ACCESS,
} from '@honuware/ui/access';

interface Assignment {
  id: string;
  personId: string;
  personLabel: string;
}

// Bespoke "assign people to a role" page (the /admin Roles & Permissions tile). The
// role_assignments generic CRUD editor is unusable — the framework marks both FK
// columns readonly, so the New form renders empty — so this talks to the generic CRUD
// ACCESS layer directly: list roles, list a role's people (resolving person_id → name
// via the people display template), delete an assignment, and add one via a person
// autocomplete. addItem/deleteItem ignore the form's readonly hints.
@Component({
  selector: 'app-manage-roles',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatFormFieldModule,
    MatSelectModule,
    MatInputModule,
    MatAutocompleteModule,
    MatButtonModule,
    MatIconModule,
    MatListModule,
    MatProgressSpinnerModule,
  ],
  templateUrl: './manage-roles.html',
  styleUrl: './manage-roles.scss',
})
export class ManageRolesPage implements OnInit {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private destroyRef = inject(DestroyRef);

  readonly roles = signal<FkOption[]>([]);
  readonly assignments = signal<Assignment[]>([]);
  readonly peopleOptions = signal<FkOption[]>([]);
  readonly selectedPerson = signal<FkOption | null>(null);
  readonly loadingAssignments = signal(false);
  readonly adding = signal(false);
  readonly error = signal('');

  readonly roleControl = new FormControl<string>('', { nonNullable: true });
  readonly personControl = new FormControl<string | FkOption>('', { nonNullable: true });

  ngOnInit(): void {
    this.loadRoles();

    // Changing the selected role reloads its people.
    this.roleControl.valueChanges
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe(() => this.loadAssignments());

    // Person autocomplete: search people, excluding those already in this role.
    // Typing (a string value) invalidates a prior pick; selecting an option emits an
    // FkOption (not a string), so it doesn't re-search.
    this.personControl.valueChanges
      .pipe(
        filter((value): value is string => typeof value === 'string'),
        tap(() => this.selectedPerson.set(null)),
        map((value) => value.trim()),
        debounceTime(200),
        distinctUntilChanged(),
        switchMap((text) =>
          text
            ? this.crud.getFkOptions('people', text, 10)
            : of<FkOptionsResponse>({ total_count: 0, options: [] }),
        ),
        map((res) => {
          const assigned = new Set(this.assignments().map((a) => a.personId));
          return res.options.filter((option) => !assigned.has(option.value));
        }),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe((options) => this.peopleOptions.set(options));
  }

  displayPerson(option: FkOption | string): string {
    return typeof option === 'string' ? option : (option?.display ?? '');
  }

  onPersonSelected(event: MatAutocompleteSelectedEvent): void {
    this.selectedPerson.set(event.option.value as FkOption);
  }

  addAssignment(): void {
    const roleId = this.roleControl.value;
    const person = this.selectedPerson();
    if (!roleId || !person) {
      return;
    }
    this.adding.set(true);
    this.error.set('');
    this.crud
      .addItem({
        table_name: 'role_assignments',
        value: { person_id: person.value, role_id: roleId },
      })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.resetPersonInput();
          this.adding.set(false);
          this.loadAssignments();
        },
        error: () => {
          this.adding.set(false);
          this.error.set('Could not add that person — they may already have this role.');
        },
      });
  }

  removeAssignment(assignment: Assignment): void {
    this.crud
      .deleteItem('role_assignments', 'id', assignment.id)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => this.loadAssignments(),
        error: () => this.loadAssignments(),
      });
  }

  private loadRoles(): void {
    this.crud
      .getFkOptions('roles', '', 200)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((res) => this.roles.set(res.options));
  }

  private loadAssignments(): void {
    const roleId = this.roleControl.value;
    if (!roleId) {
      this.assignments.set([]);
      return;
    }
    this.loadingAssignments.set(true);
    this.crud
      .getFilteredTableRows('role_assignments', 'id', true, 1000, 0, [
        { column_name: 'role_id', column_value: roleId },
      ])
      .pipe(
        switchMap((result) => {
          const rows = this.parseRows(result);
          const personIds = [...new Set(rows.map((r) => r['person_id']).filter(Boolean))];
          const resolved$ = personIds.length
            ? this.crud.resolveFkDisplay('people', personIds).pipe(map((r) => r.resolved))
            : of<Record<string, string>>({});
          return resolved$.pipe(
            map((resolved) =>
              rows.map((row) => ({
                id: row['id'],
                personId: row['person_id'],
                personLabel: resolved[row['person_id']] ?? `Person #${row['person_id']}`,
              })),
            ),
          );
        }),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe({
        next: (assignments) => {
          this.assignments.set(assignments);
          this.loadingAssignments.set(false);
        },
        error: () => {
          this.assignments.set([]);
          this.loadingAssignments.set(false);
        },
      });
  }

  private resetPersonInput(): void {
    this.personControl.setValue('');
    this.selectedPerson.set(null);
    this.peopleOptions.set([]);
  }

  // Turn a {sortedColumnNames, dataTable} result into column→value row objects.
  private parseRows(result: DataResultsWithCount): Record<string, string>[] {
    return result.dataTable.map((row) => {
      const obj: Record<string, string> = {};
      result.sortedColumnNames.forEach((col, idx) => {
        obj[col] = row[idx];
      });
      return obj;
    });
  }
}

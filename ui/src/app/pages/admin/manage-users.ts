import { Component, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { RouterLink } from '@angular/router';
import { FormControl, FormGroup, ReactiveFormsModule, Validators } from '@angular/forms';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import {
  debounceTime,
  distinctUntilChanged,
  filter,
  map,
  of,
  switchMap,
} from 'rxjs';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import {
  MatAutocompleteModule,
  MatAutocompleteSelectedEvent,
} from '@angular/material/autocomplete';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatCardModule } from '@angular/material/card';
import {
  DataResults,
  FkOption,
  FkOptionsResponse,
  HONUWARE_CRUD_ACCESS,
} from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { ADMIN_USERS_ACCESS } from '../../core/admin-users-access/admin-users-access';

interface UserRow {
  id: string;
  first_name: string;
  last_name: string;
  email: string;
}

// Bespoke "Users" admin page (/admin/users). Search accounts by first/last/email in
// one autocomplete (getFkOptions matches any of the three columns), then edit the
// name/email, reset the password, or delete the account. Search/edit/delete talk to
// HONUWARE_CRUD_ACCESS directly: getFkOptions (search) → getRow (load) → updateItem /
// deleteItem. Delete cascades to the person's sessions/roles/tokens server-side; we
// guard against self-deletion. Create-user + reset-password are the two bespoke,
// admin_portal-gated actions the generic CRUD doesn't cover — they go through the
// ADMIN_USERS_ACCESS seam (POST /api/admin/create_user, /api/admin/reset_password),
// each of which emails the person a temporary password.
@Component({
  selector: 'app-manage-users',
  imports: [
    RouterLink,
    ReactiveFormsModule,
    MatFormFieldModule,
    MatInputModule,
    MatAutocompleteModule,
    MatButtonModule,
    MatIconModule,
    MatCardModule,
  ],
  templateUrl: './manage-users.html',
  styleUrl: './manage-users.scss',
})
export class ManageUsersPage implements OnInit {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private adminUsers = inject(ADMIN_USERS_ACCESS);
  private auth = inject(AuthService);
  private destroyRef = inject(DestroyRef);

  readonly peopleOptions = signal<FkOption[]>([]);
  readonly selectedUser = signal<UserRow | null>(null);
  readonly saving = signal(false);
  readonly message = signal('');
  readonly error = signal('');

  // Create-account form state is kept separate from the edit/search flow so the two
  // sections' confirmations and spinners never clobber each other.
  readonly creating = signal(false);
  readonly createMessage = signal('');
  readonly createError = signal('');

  readonly searchControl = new FormControl<string | FkOption>('', { nonNullable: true });
  readonly editForm = new FormGroup({
    first_name: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    last_name: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    email: new FormControl('', {
      nonNullable: true,
      validators: [Validators.required, Validators.email],
    }),
  });
  readonly createForm = new FormGroup({
    first_name: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    last_name: new FormControl('', { nonNullable: true, validators: [Validators.required] }),
    email: new FormControl('', {
      nonNullable: true,
      validators: [Validators.required, Validators.email],
    }),
  });

  ngOnInit(): void {
    // Autocomplete: search as you type. A selected option emits an FkOption (not a
    // string), so the filter below stops it from re-searching.
    this.searchControl.valueChanges
      .pipe(
        filter((value): value is string => typeof value === 'string'),
        map((value) => value.trim()),
        debounceTime(200),
        distinctUntilChanged(),
        switchMap((text) =>
          text
            ? this.crud.getFkOptions('people', text, 10)
            : of<FkOptionsResponse>({ total_count: 0, options: [] }),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe((res) => this.peopleOptions.set(res.options));
  }

  displayPerson(option: FkOption | string): string {
    return typeof option === 'string' ? option : (option?.display ?? '');
  }

  onUserSelected(event: MatAutocompleteSelectedEvent): void {
    this.loadUser((event.option.value as FkOption).value);
  }

  save(): void {
    const user = this.selectedUser();
    if (!user || this.editForm.invalid) {
      this.editForm.markAllAsTouched();
      return;
    }
    this.saving.set(true);
    this.message.set('');
    this.error.set('');
    const value = this.editForm.getRawValue();
    this.crud
      .updateItem({ table_name: 'people', column_name: 'id', column_value: user.id, value })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.saving.set(false);
          // Return to the search view (the "user management" landing) with a
          // confirmation, instead of stranding the admin on the edit form.
          this.clearSelection();
          this.message.set('Account updated.');
        },
        error: () => {
          this.saving.set(false);
          this.error.set('Could not save — that email may already be in use.');
        },
      });
  }

  resetPassword(): void {
    const user = this.selectedUser();
    if (!user) {
      return;
    }
    const confirmed = window.confirm(
      `Email a temporary password to ${user.email}?\n\n` +
        'They will be required to set a new password on their next login.',
    );
    if (!confirmed) {
      return;
    }
    this.saving.set(true);
    this.message.set('');
    this.error.set('');
    this.adminUsers
      .resetPassword(user.email)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.saving.set(false);
          this.message.set(`A temporary password was emailed to ${user.email}.`);
        },
        error: () => {
          this.saving.set(false);
          this.error.set('Could not reset that password.');
        },
      });
  }

  createUser(): void {
    if (this.createForm.invalid) {
      this.createForm.markAllAsTouched();
      return;
    }
    this.creating.set(true);
    this.createMessage.set('');
    this.createError.set('');
    const input = this.createForm.getRawValue();
    this.adminUsers
      .createUser(input)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          this.creating.set(false);
          this.createForm.reset({ first_name: '', last_name: '', email: '' });
          if (result.already_exists) {
            this.createMessage.set(
              `An account for ${result.first_name} ${result.last_name} already exists — ` +
                'no email was sent.',
            );
          } else {
            this.createMessage.set(
              `Account created for ${result.first_name} ${result.last_name}. ` +
                `A temporary password was emailed to ${input.email} to sign in.`,
            );
          }
        },
        error: () => {
          this.creating.set(false);
          this.createError.set('Could not create that account — please try again.');
        },
      });
  }

  deleteUser(): void {
    const user = this.selectedUser();
    if (!user) {
      return;
    }
    // Guard: an admin deleting their own account would cascade-delete their session
    // and lock them out mid-action.
    const authData = this.auth.authData;
    if (authData.isAuth && String(authData.personId) === user.id) {
      this.error.set('You cannot delete your own account.');
      return;
    }
    const confirmed = window.confirm(
      `Delete ${user.first_name} ${user.last_name} (${user.email})?\n\n` +
        'This also removes their roles and active sessions and cannot be undone.',
    );
    if (!confirmed) {
      return;
    }
    this.saving.set(true);
    this.message.set('');
    this.error.set('');
    this.crud
      .deleteItem('people', 'id', user.id)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: () => {
          this.saving.set(false);
          this.clearSelection();
          this.message.set('Account deleted.');
        },
        error: () => {
          this.saving.set(false);
          this.error.set('Could not delete that account.');
        },
      });
  }

  private loadUser(id: string): void {
    this.message.set('');
    this.error.set('');
    this.crud
      .getRow('people', 'id', id)
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (result) => {
          const row = this.firstRow(result);
          if (!row) {
            this.error.set('Could not load that account.');
            return;
          }
          const user: UserRow = {
            id: row['id'] ?? id,
            first_name: row['first_name'] ?? '',
            last_name: row['last_name'] ?? '',
            email: row['email'] ?? '',
          };
          this.selectedUser.set(user);
          this.editForm.setValue({
            first_name: user.first_name,
            last_name: user.last_name,
            email: user.email,
          });
        },
        error: () => this.error.set('Could not load that account.'),
      });
  }

  private clearSelection(): void {
    this.selectedUser.set(null);
    this.searchControl.setValue('');
    this.peopleOptions.set([]);
    this.editForm.reset({ first_name: '', last_name: '', email: '' });
  }

  private firstRow(result: DataResults): Record<string, string> | null {
    if (!result.dataTable.length) {
      return null;
    }
    const obj: Record<string, string> = {};
    result.sortedColumnNames.forEach((col, idx) => {
      obj[col] = result.dataTable[0][idx];
    });
    return obj;
  }
}

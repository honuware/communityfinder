import { TestBed } from '@angular/core/testing';
import { vi } from 'vitest';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { ADMIN_USERS_ACCESS } from '../../core/admin-users-access/admin-users-access';
import { ManageUsersPage } from './manage-users';

function crudStub() {
  return {
    getFkOptions: () =>
      of({ total_count: 1, options: [{ value: '2', display: 'Ada Lovelace - ada@example.com' }] }),
    getRow: () =>
      of({
        sortedColumnNames: ['id', 'email', 'first_name', 'last_name'],
        dataTable: [['2', 'ada@example.com', 'Ada', 'Lovelace']],
      }),
    updateItem: () => of(void 0),
    deleteItem: () => of(void 0),
  };
}

function adminUsersStub(alreadyExists = false) {
  return {
    createUser: (input: { first_name: string; last_name: string; email: string }) =>
      of({
        person_id: 99,
        already_exists: alreadyExists,
        first_name: input.first_name,
        last_name: input.last_name,
      }),
    resetPassword: () => of(void 0),
  };
}

describe('ManageUsersPage', () => {
  function configure(personId = 1, adminUsers = adminUsersStub()) {
    TestBed.configureTestingModule({
      imports: [ManageUsersPage],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        { provide: HONUWARE_CRUD_ACCESS, useValue: crudStub() },
        { provide: ADMIN_USERS_ACCESS, useValue: adminUsers },
        { provide: AuthService, useValue: { authData: { isAuth: true, personId } } },
      ],
    });
  }

  it('loads a selected account into the edit form', () => {
    configure();
    const fixture = TestBed.createComponent(ManageUsersPage);
    fixture.detectChanges();
    // Simulate the autocomplete option selection.
    fixture.componentInstance.onUserSelected({
      option: { value: { value: '2', display: 'Ada Lovelace - ada@example.com' } },
    } as never);
    expect(fixture.componentInstance.selectedUser()?.email).toBe('ada@example.com');
    expect(fixture.componentInstance.editForm.getRawValue()).toEqual({
      first_name: 'Ada',
      last_name: 'Lovelace',
      email: 'ada@example.com',
    });
  });

  it('refuses to delete your own account', () => {
    configure(1);
    const fixture = TestBed.createComponent(ManageUsersPage);
    fixture.detectChanges();
    // A selected user whose id matches the logged-in personId (1).
    fixture.componentInstance.selectedUser.set({
      id: '1',
      first_name: 'Me',
      last_name: 'Self',
      email: 'me@example.com',
    });
    fixture.componentInstance.deleteUser();
    expect(fixture.componentInstance.error()).toContain('cannot delete your own account');
  });

  it('creates a user and confirms a temporary password was emailed', () => {
    configure();
    const fixture = TestBed.createComponent(ManageUsersPage);
    fixture.detectChanges();
    fixture.componentInstance.createForm.setValue({
      first_name: 'Grace',
      last_name: 'Hopper',
      email: 'grace@example.com',
    });
    fixture.componentInstance.createUser();
    expect(fixture.componentInstance.createMessage()).toContain('temporary password was emailed');
    // The create form is cleared, ready for the next entry.
    expect(fixture.componentInstance.createForm.getRawValue().email).toBe('');
  });

  it('reports when a created account already exists', () => {
    configure(1, adminUsersStub(true));
    const fixture = TestBed.createComponent(ManageUsersPage);
    fixture.detectChanges();
    fixture.componentInstance.createForm.setValue({
      first_name: 'Ada',
      last_name: 'Lovelace',
      email: 'ada@example.com',
    });
    fixture.componentInstance.createUser();
    expect(fixture.componentInstance.createMessage()).toContain('already exists');
  });

  it('resets a password and confirms the email', () => {
    vi.spyOn(window, 'confirm').mockReturnValue(true);
    configure();
    const fixture = TestBed.createComponent(ManageUsersPage);
    fixture.detectChanges();
    fixture.componentInstance.selectedUser.set({
      id: '2',
      first_name: 'Ada',
      last_name: 'Lovelace',
      email: 'ada@example.com',
    });
    fixture.componentInstance.resetPassword();
    expect(fixture.componentInstance.message()).toContain('temporary password was emailed');
  });
});

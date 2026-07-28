import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { ManageRolesPage } from './manage-roles';

// A minimal CrudAccess stub returning synchronous observables so the page loads
// deterministically: two roles, one assignment resolving to a named person.
function crudStub() {
  return {
    getFkOptions: (tableName: string) =>
      of({
        total_count: 2,
        options:
          tableName === 'roles'
            ? [
                { value: '1', display: 'admin' },
                { value: '2', display: 'user' },
              ]
            : [{ value: '2', display: 'Ada Lovelace - ada@example.com' }],
      }),
    getFilteredTableRows: () =>
      of({
        sortedColumnNames: ['id', 'person_id', 'role_id'],
        dataTable: [['5', '2', '1']],
        totalCount: 1,
      }),
    resolveFkDisplay: () => of({ resolved: { '2': 'Ada Lovelace - ada@example.com' } }),
    addItem: () => of(void 0),
    deleteItem: () => of(void 0),
  };
}

describe('ManageRolesPage', () => {
  function configure() {
    TestBed.configureTestingModule({
      imports: [ManageRolesPage],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        { provide: HONUWARE_CRUD_ACCESS, useValue: crudStub() },
      ],
    });
  }

  it('loads roles into the dropdown', () => {
    configure();
    const fixture = TestBed.createComponent(ManageRolesPage);
    fixture.detectChanges();
    expect(fixture.componentInstance.roles().map((r) => r.display)).toEqual(['admin', 'user']);
  });

  it('lists the people assigned to the selected role with resolved names', () => {
    configure();
    const fixture = TestBed.createComponent(ManageRolesPage);
    fixture.detectChanges();
    fixture.componentInstance.roleControl.setValue('1');
    fixture.detectChanges();
    const assignments = fixture.componentInstance.assignments();
    expect(assignments.length).toBe(1);
    expect(assignments[0].id).toBe('5');
    expect(assignments[0].personLabel).toBe('Ada Lovelace - ada@example.com');
  });
});

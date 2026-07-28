import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of } from 'rxjs';
import { DatabaseSchemaService } from '@honuware/ui/crud';
import { AdminPage } from './admin';
import { ADMIN_MOCK_SCHEMA } from '../../core/mock/honuware-mock';

// The dashboard reads the schema through DatabaseSchemaService. We stub it with a
// synchronous of(schema) so the test is deterministic (the real service self-fetches
// + polls); the mock schema is the same one the offline build serves.
function stubSchemaService() {
  return {
    schema$: of(ADMIN_MOCK_SCHEMA),
    GetDBSchema: () => of(ADMIN_MOCK_SCHEMA),
    refreshSchema: () => {},
  };
}

describe('AdminPage', () => {
  function configure() {
    TestBed.configureTestingModule({
      imports: [AdminPage],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        { provide: DatabaseSchemaService, useValue: stubSchemaService() },
      ],
    });
  }

  it('creates and loads the root tables from the schema', () => {
    configure();
    const fixture = TestBed.createComponent(AdminPage);
    fixture.detectChanges();
    const component = fixture.componentInstance;
    expect(component).toBeTruthy();
    expect(component.schema().root_tables).toContain('people');
    expect(component.schema().root_tables).toContain('roles');
  });

  it('resolves friendly names from the schema, falling back to a title-cased name', () => {
    configure();
    const fixture = TestBed.createComponent(AdminPage);
    fixture.detectChanges();
    const component = fixture.componentInstance;
    expect(component.friendlyName('people')).toBe('People');
    expect(component.friendlyName('event_sessions')).toBe('Event sessions');
  });
});

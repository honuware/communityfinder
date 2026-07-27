import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideHttpClient } from '@angular/common/http';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { of } from 'rxjs';
import { Header } from './header';

// The header reads AuthService.authData$ and swaps between Login/Register links
// (logged out) and a user chip + dropdown (logged in). Both paths are covered by
// stubbing AuthService directly so the spec does not depend on the mock's default
// auth state.
describe('Header', () => {
  function configure(authStub: Partial<AuthService>) {
    TestBed.configureTestingModule({
      imports: [Header],
      providers: [
        provideRouter([]),
        provideNoopAnimations(),
        provideHttpClient(),
        { provide: HONUWARE_API_BASE, useValue: '/api' },
        ...provideHonuwareAccessMock(),
        { provide: AuthService, useValue: authStub },
      ],
    });
  }

  it('shows Login/Register when logged out', () => {
    configure({ authData$: of({ isAuth: false }) } as unknown as AuthService);
    const fixture = TestBed.createComponent(Header);
    fixture.detectChanges();
    const text = (fixture.nativeElement as HTMLElement).textContent ?? '';
    expect(fixture.componentInstance).toBeTruthy();
    expect(text).toContain('Login');
    expect(text).toContain('Register');
  });

  it('shows the user name when logged in', () => {
    const user = {
      isAuth: true,
      personId: 7,
      firstName: 'Ada',
      lastName: 'Lovelace',
      email: 'ada@example.com',
      createdAt: '2026-01-01',
      isAdmin: false,
      roles: [],
      permissions: [],
      mustChangePassword: false,
    };
    configure({ authData$: of(user) } as unknown as AuthService);
    const fixture = TestBed.createComponent(Header);
    fixture.detectChanges();
    const text = (fixture.nativeElement as HTMLElement).textContent ?? '';
    expect(text).toContain('Ada');
    expect(text).toContain('Lovelace');
    expect(text).not.toContain('Login');
  });
});

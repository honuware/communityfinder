import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter } from '@angular/router';
import { AuthService } from '@honuware/ui/auth';
import type { AuthData } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { BehaviorSubject, of } from 'rxjs';
import { AccountPage } from './account';

// The real AuthService reads from the mock auth access, which starts logged-out.
// To exercise the logged-in path we replace AuthService with a stub whose
// authData/authData$ report an authenticated user.
const authedUser: AuthData = {
  isAuth: true,
  personId: 42,
  firstName: 'Mason',
  lastName: 'Bendixen',
  email: 'masonbendixen@gmail.com',
  createdAt: '2026-01-01T00:00:00Z',
  isAdmin: false,
  roles: [],
  permissions: [],
  mustChangePassword: false,
};

describe('AccountPage', () => {
  let fixture: ComponentFixture<AccountPage>;
  let authStub: {
    authData: AuthData;
    authData$: BehaviorSubject<AuthData>;
    setUserInfo: ReturnType<typeof vi.fn>;
  };

  beforeEach(async () => {
    authStub = {
      authData: authedUser,
      authData$: new BehaviorSubject<AuthData>(authedUser),
      setUserInfo: vi.fn(() => of(void 0)),
    };

    await TestBed.configureTestingModule({
      imports: [AccountPage],
      providers: [
        // Provides the HONUWARE_*_ACCESS mocks the embedded hw-photo-upload needs.
        provideHonuwareAccessMock(),
        provideNoopAnimations(),
        provideRouter([]),
        { provide: AuthService, useValue: authStub },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(AccountPage);
    fixture.detectChanges();
  });

  it('creates', () => {
    expect(fixture.componentInstance).toBeTruthy();
  });

  it("shows the user's email", () => {
    const email: HTMLInputElement = fixture.nativeElement.querySelector('#email');
    expect(email.value).toBe('masonbendixen@gmail.com');
  });

  it('clicking Save calls setUserInfo', () => {
    const saveButton: HTMLButtonElement = fixture.nativeElement.querySelector('#save');
    saveButton.click();
    fixture.detectChanges();

    expect(authStub.setUserInfo).toHaveBeenCalledWith(
      'Mason',
      'Bendixen',
      'masonbendixen@gmail.com',
    );
  });
});

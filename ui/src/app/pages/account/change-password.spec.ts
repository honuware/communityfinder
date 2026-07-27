import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { provideRouter, Router } from '@angular/router';
import { AuthService } from '@honuware/ui/auth';
import type { AuthData } from '@honuware/ui/auth';
import { BehaviorSubject, of } from 'rxjs';
import { ChangePasswordPage } from './change-password';

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

describe('ChangePasswordPage', () => {
  let fixture: ComponentFixture<ChangePasswordPage>;
  let component: ChangePasswordPage;
  let authStub: {
    authData: AuthData;
    authData$: BehaviorSubject<AuthData>;
    updateUserPassword: ReturnType<typeof vi.fn>;
  };

  beforeEach(async () => {
    authStub = {
      authData: authedUser,
      authData$: new BehaviorSubject<AuthData>(authedUser),
      updateUserPassword: vi.fn(() => of(void 0)),
    };

    await TestBed.configureTestingModule({
      imports: [ChangePasswordPage],
      providers: [
        provideNoopAnimations(),
        provideRouter([]),
        { provide: AuthService, useValue: authStub },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ChangePasswordPage);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('creates', () => {
    expect(component).toBeTruthy();
  });

  it('mismatched new/confirm blocks submit', () => {
    component.form.setValue({
      currentPassword: 'oldpass',
      newPassword: 'newpass1',
      confirmPassword: 'newpass2',
    });
    fixture.detectChanges();

    const saveButton: HTMLButtonElement = fixture.nativeElement.querySelector('#save');
    saveButton.click();
    fixture.detectChanges();

    expect(authStub.updateUserPassword).not.toHaveBeenCalled();
  });

  it('a valid submit calls updateUserPassword', () => {
    const router = TestBed.inject(Router);
    const navigateSpy = vi.spyOn(router, 'navigate').mockResolvedValue(true);

    component.form.setValue({
      currentPassword: 'oldpass',
      newPassword: 'newpass',
      confirmPassword: 'newpass',
    });
    fixture.detectChanges();

    const saveButton: HTMLButtonElement = fixture.nativeElement.querySelector('#save');
    saveButton.click();
    fixture.detectChanges();

    expect(authStub.updateUserPassword).toHaveBeenCalledWith('oldpass', 'newpass');
    expect(navigateSpy).toHaveBeenCalledWith(['/account']);
  });
});

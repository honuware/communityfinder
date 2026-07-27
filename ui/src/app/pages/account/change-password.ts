import { Component, inject, signal } from '@angular/core';
import {
  AbstractControl,
  FormBuilder,
  ReactiveFormsModule,
  ValidationErrors,
  Validators,
} from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { Router, RouterLink } from '@angular/router';
import { AuthService } from '@honuware/ui/auth';

// Form-level validator: the new password and its confirmation must match. Emits
// a `passwordMismatch` error on the group when they differ.
function passwordsMatchValidator(group: AbstractControl): ValidationErrors | null {
  const newPassword = group.get('newPassword')?.value ?? '';
  const confirmPassword = group.get('confirmPassword')?.value ?? '';
  return newPassword === confirmPassword ? null : { passwordMismatch: true };
}

// The change-password page. Doubles as the forced-password-change destination:
// when the signed-in user has `mustChangePassword`, a prominent notice is shown.
@Component({
  selector: 'app-change-password',
  imports: [
    ReactiveFormsModule,
    RouterLink,
    MatCardModule,
    MatButtonModule,
    MatFormFieldModule,
    MatInputModule,
  ],
  templateUrl: './change-password.html',
  styleUrl: './change-password.scss',
})
export class ChangePasswordPage {
  private fb = inject(FormBuilder);
  private authService = inject(AuthService);
  private router = inject(Router);

  readonly form = this.fb.group(
    {
      currentPassword: ['', [Validators.required]],
      newPassword: ['', [Validators.required]],
      confirmPassword: ['', [Validators.required]],
    },
    { validators: passwordsMatchValidator },
  );

  // The app routes forced-password-change users here; surface the notice when
  // the signed-in user still has must_change_password set.
  private readonly auth = this.authService.authData;
  readonly mustChangePassword = this.auth.isAuth && this.auth.mustChangePassword;

  readonly saving = signal(false);
  readonly errored = signal(false);
  readonly success = signal(false);

  get passwordsMismatch(): boolean {
    return this.form.hasError('passwordMismatch');
  }

  onSave(): void {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      return;
    }

    this.saving.set(true);
    this.errored.set(false);
    this.success.set(false);

    const currentPassword = this.form.get('currentPassword')?.value ?? '';
    const newPassword = this.form.get('newPassword')?.value ?? '';

    this.authService.updateUserPassword(currentPassword, newPassword).subscribe({
      next: () => this.onSuccess(),
      error: () => this.showError(),
    });
  }

  private onSuccess(): void {
    this.saving.set(false);
    this.success.set(true);
    this.router.navigate(['/account']);
  }

  private showError(): void {
    this.saving.set(false);
    this.errored.set(true);
  }
}

import { Component, inject, signal } from '@angular/core';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { RouterLink } from '@angular/router';
import { AuthService } from '@honuware/ui/auth';
import { PhotoUploadComponent } from '@honuware/ui/photos';

// The account / profile page. Reads the signed-in user from the honuware
// AuthService, lets them edit their name + email (setUserInfo) and manage their
// profile photo (hw-photo-upload), and links out to the change-password page.
@Component({
  selector: 'app-account',
  imports: [
    ReactiveFormsModule,
    RouterLink,
    MatCardModule,
    MatButtonModule,
    MatFormFieldModule,
    MatInputModule,
    PhotoUploadComponent,
  ],
  templateUrl: './account.html',
  styleUrl: './account.scss',
})
export class AccountPage {
  private fb = inject(FormBuilder);
  private authService = inject(AuthService);

  // The /account route is AuthGuard-protected, so authData is normally
  // authenticated. Narrow on `isAuth` before reading the user fields (TS
  // strict) and default personId to 0 for the unreachable logged-out case —
  // the template renders nothing then anyway.
  private readonly auth = this.authService.authData;
  readonly isAuth = this.auth.isAuth;
  readonly personId = this.auth.isAuth ? this.auth.personId : 0;

  readonly form = this.fb.group({
    firstName: ['', [Validators.required]],
    lastName: ['', [Validators.required]],
    email: ['', [Validators.required, Validators.email]],
  });

  readonly saving = signal(false);
  readonly saved = signal(false);
  readonly errored = signal(false);

  constructor() {
    if (this.auth.isAuth) {
      this.form.patchValue({
        firstName: this.auth.firstName,
        lastName: this.auth.lastName,
        email: this.auth.email,
      });
    }
  }

  onSave(): void {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      return;
    }

    this.saving.set(true);
    this.saved.set(false);
    this.errored.set(false);

    const firstName = this.form.get('firstName')?.value ?? '';
    const lastName = this.form.get('lastName')?.value ?? '';
    const email = this.form.get('email')?.value ?? '';

    this.authService.setUserInfo(firstName, lastName, email).subscribe({
      next: () => this.showSaved(),
      error: () => this.showError(),
    });
  }

  private showSaved(): void {
    this.saving.set(false);
    this.saved.set(true);
    this.errored.set(false);
  }

  private showError(): void {
    this.saving.set(false);
    this.saved.set(false);
    this.errored.set(true);
  }
}

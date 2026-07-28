import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { AdminDashboard } from './admin-dashboard';

describe('AdminDashboard', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [AdminDashboard],
      providers: [provideRouter([]), provideNoopAnimations()],
    });
  });

  it('renders the Roles & Permissions tile linking to the bespoke role page', () => {
    const fixture = TestBed.createComponent(AdminDashboard);
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent ?? '').toContain('Roles & Permissions');
    const hrefs = Array.from(el.querySelectorAll('a')).map((a) => a.getAttribute('href'));
    expect(hrefs).toContain('/admin/roles');
  });
});

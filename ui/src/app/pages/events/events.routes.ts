import { Routes } from '@angular/router';
import { EventDetailPage } from './event-detail';
import { EventsCalendarPage } from './events-calendar';
import { EventsListPage } from './events-list';
import { SeriesDetailPage } from './series-detail';
import { SeriesIndexPage } from './series-index';
import { SubmitEventPage } from './submit-event';

// Mounted at /events, lazy-loaded so the events pages stay out of the
// initial bundle. ':id' must come last: listed earlier it would match
// /events/calendar, /events/series and /events/submit as event ids (the
// knottyyoga route-ordering hazard).
const routes: Routes = [
  { path: '', component: EventsListPage },
  { path: 'calendar', component: EventsCalendarPage },
  { path: 'series', component: SeriesIndexPage },
  { path: 'series/:slug', component: SeriesDetailPage },
  // The submit form (EV3 §5.2). Open to everyone: signed out, it explains how
  // to sign in.
  { path: 'submit', component: SubmitEventPage },
  { path: ':id', component: EventDetailPage },
];

export default routes;

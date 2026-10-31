import { Routes } from '@angular/router';
import { provideMarkdown } from 'ngx-markdown';
import { environment } from '../../../environments/environment';
import { provideGuideAccess } from '../../core/guide-access/guide-access.providers';
import { GuideVocabulariesService } from '../../core/services/guide-vocabularies.service';
import { GraveyardPage } from './graveyard-page';
import { GuideArticlePage } from './guide-article';
import { GuideHomePage } from './guide-home';
import { GuideShell } from './guide-shell';
import { NeighborhoodPage } from './neighborhood-page';
import { NeighborhoodsPage } from './neighborhoods-page';
import { OrganizationDirectoryPage } from './organization-directory';
import { OrganizationProfilePage } from './organization-profile';
import { PlacePage } from './place-page';
import { SceneLandingPage } from './scene-landing';
import { SectionIndexPage } from './section-index';
import { ServicesPage } from './services-page';
import { VenueByIdNotFoundPage, venueByIdRedirect } from './venue-by-id';
import { VenueDirectoryPage } from './venue-directory';
import { VenueProfilePage } from './venue-profile';

// Mounted at /guide and lazy-loaded. Everything the guide needs comes with
// it: its data seam (with the offline demo data in mock mode), the scene and
// neighborhood names, and the markdown renderer. None of it is in the initial
// bundle (GD3 OQ3, OQ7 (d)).
//
// In each group the literal paths come before the :slug ones (the knottyyoga
// route-ordering hazard). `useMock` is a parameter so specs can mount the
// real routes over the mock data.
export function guideRoutes(useMock: boolean): Routes {
  return [
    {
      path: '',
      component: GuideShell,
      providers: [provideGuideAccess(useMock), GuideVocabulariesService, provideMarkdown()],
      children: [
        { path: '', component: GuideHomePage },
        { path: 'venues', component: VenueDirectoryPage },
        {
          path: 'venues/by-id/:venueId',
          canActivate: [venueByIdRedirect],
          component: VenueByIdNotFoundPage,
        },
        { path: 'venues/:slug', component: VenueProfilePage },
        { path: 'orgs', component: OrganizationDirectoryPage },
        { path: 'orgs/:slug', component: OrganizationProfilePage },
        { path: 'places/:slug', component: PlacePage },
        { path: 'services', component: ServicesPage },
        { path: 'neighborhoods', component: NeighborhoodsPage },
        { path: 'neighborhoods/:slug', component: NeighborhoodPage },
        { path: 'graveyard', component: GraveyardPage },
        { path: 'scenes/:slug', component: SceneLandingPage },
        { path: 'sections/:section', component: SectionIndexPage },
        { path: 'pages/:slug', component: GuideArticlePage },
      ],
    },
  ];
}

export default guideRoutes(environment.useMock);

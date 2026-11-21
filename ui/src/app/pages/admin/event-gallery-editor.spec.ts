import { TestBed } from '@angular/core/testing';
import { By } from '@angular/platform-browser';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { firstValueFrom, throwError } from 'rxjs';
import { vi } from 'vitest';
import { HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { ToastService } from '@honuware/ui/foundation';
import { AuthService } from '@honuware/ui/auth';
import { PhotoUploadComponent } from '@honuware/ui/photos';
import { HonuwareAccessMocks, provideHonuwareAccessMock } from '@honuware/ui/testing';
import { EVENTS_ADMIN_ACCESS } from '../../core/events-admin-access/events-admin-access';
import { provideEventsAdminAccess } from '../../core/events-admin-access/events-admin-access.providers';
import { GallerySettingsMockStore } from '../../core/mock/gallery-settings-mock-store';
import { HONUWARE_MOCK_OPTIONS } from '../../core/mock/honuware-mock';
import { EventPhotos, GALLERY_THUMB_SIZE } from '../../core/services/event-photos.service';
import { dataResultsToRecords } from '../../core/types/events.types';
import { EventGalleryEditor } from './event-gallery-editor';

// In the seed, Leather Night (103) has a gallery of four (ids 1–4, sort_order
// 1–4); Ladies Night (101) has none.
const LEATHER_NIGHT = 103;
const NO_GALLERY = 101;
const TABLE = 'event_gallery_photos';

function create(
  eventId = LEATHER_NIGHT,
  before: (mocks: HonuwareAccessMocks, store: GallerySettingsMockStore) => void = () => undefined,
) {
  let mocks!: HonuwareAccessMocks;
  TestBed.configureTestingModule({
    providers: [
      provideNoopAnimations(),
      provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, (captured) => (mocks = captured)),
      provideEventsAdminAccess(true),
      { provide: AuthService, useValue: { authData: { isAuth: true, personId: 1 } } },
    ],
  });
  // Makes the mocks, so `before` can seed them.
  TestBed.inject(HONUWARE_CRUD_ACCESS);
  before(mocks, TestBed.inject(GallerySettingsMockStore));
  const toast = TestBed.inject(ToastService);
  const success = vi.spyOn(toast, 'success');
  const error = vi.spyOn(toast, 'error');
  const fixture = TestBed.createComponent(EventGalleryEditor);
  fixture.componentRef.setInput('eventId', eventId);
  fixture.detectChanges();
  fixture.detectChanges();
  const upload = fixture.debugElement.query(By.directive(PhotoUploadComponent))
    .componentInstance as PhotoUploadComponent;
  return {
    fixture,
    editor: fixture.componentInstance,
    element: fixture.nativeElement as HTMLElement,
    crud: mocks.crud,
    photo: mocks.photo,
    upload,
    success,
    error,
  };
}

function text(element: Element | null | undefined): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

function shownIds(element: HTMLElement): number[] {
  return Array.from(element.querySelectorAll('.gallery-editor__row')).map((row) =>
    Number(row.getAttribute('data-id')),
  );
}

function row(element: HTMLElement, id: number): HTMLElement {
  return element.querySelector(`.gallery-editor__row[data-id="${id}"]`)!;
}

function button(within: Element, label: string): HTMLButtonElement {
  return Array.from(within.querySelectorAll('button')).find((entry) => text(entry) === label)!;
}

async function storedOrder(crud: HonuwareAccessMocks['crud'], eventId = LEATHER_NIGHT) {
  const result = await firstValueFrom(
    crud.getFilteredTableRows(TABLE, 'id', true, 500, 0, [
      { column_name: 'event_id', column_value: String(eventId) },
    ]),
  );
  return dataResultsToRecords(result)
    .map((entry) => ({ id: Number(entry['id']), sortOrder: Number(entry['sort_order']) }))
    .sort((a, b) => a.sortOrder - b.sortOrder || a.id - b.id)
    .map((entry) => entry.id);
}

const PICTURE = () =>
  new File(['<svg xmlns="http://www.w3.org/2000/svg"/>'], 'night.svg', {
    type: 'image/svg+xml',
  });

function fillInAdd(editor: EventGalleryEditor, upload: PhotoUploadComponent) {
  editor.addForm.setValue({
    altText: ' The bar from the door ',
    caption: 'Doors at nine.',
    credit: 'Photo: the venue',
    consent: true,
  });
  upload.pendingFile = PICTURE();
}

describe('EventGalleryEditor', () => {
  afterEach(() => vi.restoreAllMocks());

  it('lists the gallery in order, each photo with its words and a thumbnail', () => {
    const { element } = create();
    expect(shownIds(element)).toEqual([1, 2, 3, 4]);
    const first = row(element, 1);
    expect(text(first.querySelector('.gallery-editor__alt'))).toBe(
      'The bar lit in deep red, with a mirror ball over the dance floor',
    );
    expect(text(first.querySelector('.gallery-editor__caption'))).toBe(
      'The main room before doors.',
    );
    expect(text(first.querySelector('.gallery-editor__credit'))).toBe('Photo: The Lantern Room');
    expect(row(element, 2).querySelector('.gallery-editor__caption')).toBeNull();
    const thumb = first.querySelector('img')!;
    expect(thumb.getAttribute('src')).toBe(
      TestBed.inject(EventPhotos).galleryPhotoUrl({ id: 1 }, GALLERY_THUMB_SIZE),
    );
    expect(thumb.getAttribute('alt')).toBe(
      'The bar lit in deep red, with a mirror ball over the dance floor',
    );
    expect(text(element.querySelector('.gallery-editor__count'))).toBe('4 of 12 photos');
  });

  it('says when there are none yet', () => {
    const { element } = create(NO_GALLERY);
    expect(shownIds(element)).toEqual([]);
    expect(text(element.querySelector('.gallery-editor__hint'))).toBe('No gallery photos yet.');
    expect(text(element.querySelector('.gallery-editor__count'))).toBe('0 of 12 photos');
  });

  describe('order', () => {
    it('moves a photo up or down by swapping two sort orders', async () => {
      const { fixture, element, crud } = create();
      const update = vi.spyOn(crud, 'updateItem');
      expect(
        row(element, 1).querySelector<HTMLButtonElement>('.gallery-editor__up')!.disabled,
      ).toBe(true);
      expect(
        row(element, 4).querySelector<HTMLButtonElement>('.gallery-editor__down')!.disabled,
      ).toBe(true);

      row(element, 3).querySelector<HTMLButtonElement>('.gallery-editor__up')!.click();
      fixture.detectChanges();
      expect(shownIds(element)).toEqual([1, 3, 2, 4]);
      expect(update.mock.calls.map(([call]) => [call.column_value, call.value])).toEqual([
        ['3', { sort_order: '2' }],
        ['2', { sort_order: '3' }],
      ]);
      expect(await storedOrder(crud)).toEqual([1, 3, 2, 4]);

      row(element, 1).querySelector<HTMLButtonElement>('.gallery-editor__down')!.click();
      fixture.detectChanges();
      expect(shownIds(element)).toEqual([3, 1, 2, 4]);
      expect(await storedOrder(crud)).toEqual([3, 1, 2, 4]);
    });

    it('renumbers the gallery when two photos share a place, so the order holds', async () => {
      const { fixture, element, crud } = create(LEATHER_NIGHT, (mocks) => {
        // Photo 3 added at the same moment as photo 2.
        mocks.crud
          .updateItem({
            table_name: TABLE,
            column_name: 'id',
            column_value: '3',
            value: { sort_order: '2' },
          })
          .subscribe();
      });
      expect(shownIds(element)).toEqual([1, 2, 3, 4]);
      row(element, 4).querySelector<HTMLButtonElement>('.gallery-editor__up')!.click();
      fixture.detectChanges();
      expect(shownIds(element)).toEqual([1, 2, 4, 3]);
      expect(await storedOrder(crud)).toEqual([1, 2, 4, 3]);
    });

    it('says so and shows the stored order when a move can’t be saved', async () => {
      const { fixture, element, crud, error } = create();
      vi.spyOn(crud, 'updateItem').mockReturnValue(throwError(() => new Error('offline')));
      row(element, 2).querySelector<HTMLButtonElement>('.gallery-editor__up')!.click();
      fixture.detectChanges();
      expect(error).toHaveBeenCalledWith("The new order couldn't be saved. Please try again.");
      expect(shownIds(element)).toEqual([1, 2, 3, 4]);
    });
  });

  // CM3 Phase 2 §10 (OQ12): a photo's words are edited where it was added.
  describe('editing the words', () => {
    async function storedWords(crud: HonuwareAccessMocks['crud'], id: number) {
      const stored = dataResultsToRecords(
        await firstValueFrom(crud.getRow(TABLE, 'id', String(id))),
      )[0];
      return { alt: stored['alt_text'], caption: stored['caption'], credit: stored['credit'] };
    }

    function editForm(element: HTMLElement, id: number): HTMLFormElement {
      return row(element, id).querySelector('.gallery-editor__edit-form') as HTMLFormElement;
    }

    it('opens the row’s alt text, caption and credit in place', () => {
      const { fixture, element, editor } = create();
      button(row(element, 1), 'Edit').click();
      fixture.detectChanges();
      expect(editForm(element, 1)).not.toBeNull();
      expect(editForm(element, 2)).toBeNull();
      expect(editor.editForm.getRawValue()).toEqual({
        altText: 'The bar lit in deep red, with a mirror ball over the dance floor',
        caption: 'The main room before doors.',
        credit: 'Photo: The Lantern Room',
      });
      expect(button(row(element, 1), 'Edit').disabled).toBe(true);
    });

    it('saves each field, trimmed, sending only what changed', async () => {
      const { fixture, element, editor, crud, success } = create();
      const update = vi.spyOn(crud, 'updateItem');
      for (const [field, column, typed, saved] of [
        ['altText', 'alt_text', '  The bar, from the door  ', 'The bar, from the door'],
        ['caption', 'caption', ' Before doors. ', 'Before doors.'],
        ['credit', 'credit', ' Photo: the venue ', 'Photo: the venue'],
      ] as const) {
        update.mockClear();
        button(row(element, 1), 'Edit').click();
        fixture.detectChanges();
        editor.editForm.controls[field].setValue(typed);
        button(editForm(element, 1), 'Save').click();
        fixture.detectChanges();
        expect(update).toHaveBeenCalledTimes(1);
        expect(update).toHaveBeenCalledWith({
          table_name: TABLE,
          column_name: 'id',
          column_value: '1',
          value: { [column]: saved },
        });
        expect(editForm(element, 1)).toBeNull();
      }
      expect(await storedWords(crud, 1)).toEqual({
        alt: 'The bar, from the door',
        caption: 'Before doors.',
        credit: 'Photo: the venue',
      });
      expect(text(row(element, 1).querySelector('.gallery-editor__alt'))).toBe(
        'The bar, from the door',
      );
      expect(text(row(element, 1).querySelector('.gallery-editor__caption'))).toBe('Before doors.');
      expect(text(row(element, 1).querySelector('.gallery-editor__credit'))).toBe(
        'Photo: the venue',
      );
      expect(success).toHaveBeenCalledWith('Photo details saved.');
    });

    it('clears a caption, and closes without writing when nothing changed', () => {
      const { fixture, element, editor, crud } = create();
      const update = vi.spyOn(crud, 'updateItem');
      button(row(element, 1), 'Edit').click();
      fixture.detectChanges();
      // Only spaces added: trimmed, it's the same, so nothing is sent.
      editor.editForm.controls.credit.setValue(' Photo: The Lantern Room ');
      button(editForm(element, 1), 'Save').click();
      fixture.detectChanges();
      expect(update).not.toHaveBeenCalled();
      expect(editForm(element, 1)).toBeNull();

      button(row(element, 1), 'Edit').click();
      fixture.detectChanges();
      editor.editForm.controls.caption.setValue('   ');
      button(editForm(element, 1), 'Save').click();
      fixture.detectChanges();
      expect(update).toHaveBeenCalledWith(expect.objectContaining({ value: { caption: '' } }));
      expect(row(element, 1).querySelector('.gallery-editor__caption')).toBeNull();
    });

    it('Cancel leaves the row as it was', () => {
      const { fixture, element, editor, crud } = create();
      const update = vi.spyOn(crud, 'updateItem');
      button(row(element, 2), 'Edit').click();
      fixture.detectChanges();
      editor.editForm.controls.altText.setValue('Something else entirely');
      button(editForm(element, 2), 'Cancel').click();
      fixture.detectChanges();
      expect(update).not.toHaveBeenCalled();
      expect(editForm(element, 2)).toBeNull();
      expect(editor.rows().find((entry) => entry.id === 2)?.altText).not.toBe(
        'Something else entirely',
      );
    });

    it('refuses blank alt text before sending anything', () => {
      const { fixture, element, editor, crud } = create();
      const update = vi.spyOn(crud, 'updateItem');
      button(row(element, 1), 'Edit').click();
      fixture.detectChanges();
      editor.editForm.controls.altText.setValue('   ');
      button(editForm(element, 1), 'Save').click();
      fixture.detectChanges();
      expect(update).not.toHaveBeenCalled();
      expect(text(editForm(element, 1).querySelector('.gallery-editor__error'))).toBe(
        'Describe the photo for people who can’t see it.',
      );
    });

    it('says so when a save fails, and keeps what was typed', () => {
      const { fixture, element, editor, crud } = create();
      vi.spyOn(crud, 'updateItem').mockReturnValue(throwError(() => new Error('offline')));
      button(row(element, 1), 'Edit').click();
      fixture.detectChanges();
      editor.editForm.controls.caption.setValue('A new caption');
      button(editForm(element, 1), 'Save').click();
      fixture.detectChanges();
      expect(text(editForm(element, 1).querySelector('.gallery-editor__error'))).toBe(
        "The changes couldn't be saved. Please try again.",
      );
      expect(editor.editForm.controls.caption.value).toBe('A new caption');
      expect(text(row(element, 1).querySelector('.gallery-editor__caption'))).toBe(
        'The main room before doors.',
      );
    });
  });

  describe('removing', () => {
    it('asks in the page first, and can be called off', () => {
      const { fixture, element, crud } = create();
      const remove = vi.spyOn(crud, 'deleteItem');
      button(row(element, 3), 'Remove').click();
      fixture.detectChanges();
      const confirm = row(element, 3).querySelector('[role="alertdialog"]')!;
      expect(text(confirm.querySelector('p'))).toBe(
        "Remove this photo from the gallery? It comes off the event's page right away.",
      );
      button(confirm, 'Keep it').click();
      fixture.detectChanges();
      expect(element.querySelector('[role="alertdialog"]')).toBeNull();
      expect(remove).not.toHaveBeenCalled();
      expect(shownIds(element)).toEqual([1, 2, 3, 4]);
    });

    it('takes down its photo, then its row', async () => {
      const { fixture, element, crud, photo, success } = create();
      await firstValueFrom(photo.uploadPhoto(TABLE, 3, new ArrayBuffer(4), 'jpeg'));
      const deletePhoto = vi.spyOn(photo, 'deletePhoto');
      const deleteRow = vi.spyOn(crud, 'deleteItem');
      button(row(element, 3), 'Remove').click();
      fixture.detectChanges();
      button(row(element, 3), 'Remove it').click();
      fixture.detectChanges();

      expect(deletePhoto).toHaveBeenCalledWith(TABLE, 3);
      expect(deleteRow).toHaveBeenCalledWith(TABLE, 'id', '3');
      expect(deletePhoto.mock.invocationCallOrder[0]).toBeLessThan(
        deleteRow.mock.invocationCallOrder[0],
      );
      expect(shownIds(element)).toEqual([1, 2, 4]);
      expect(await storedOrder(crud)).toEqual([1, 2, 4]);
      expect((await firstValueFrom(photo.hasPhoto(TABLE, 3))).has_photo).toBe(false);
      expect(success).toHaveBeenCalledWith('Photo removed.');
      expect(text(element.querySelector('.gallery-editor__count'))).toBe('3 of 12 photos');
    });

    it('still removes the row when it had no photo to take down', () => {
      const { fixture, element, photo } = create();
      vi.spyOn(photo, 'deletePhoto').mockReturnValue(throwError(() => ({ status: 404 })));
      button(row(element, 2), 'Remove').click();
      fixture.detectChanges();
      button(row(element, 2), 'Remove it').click();
      fixture.detectChanges();
      expect(shownIds(element)).toEqual([1, 3, 4]);
    });
  });

  describe('adding', () => {
    it('makes the row first, then uploads its photo against it', async () => {
      const { fixture, editor, element, crud, photo, upload, success } = create();
      const add = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'addGalleryPhoto');
      const sendPhoto = vi.spyOn(upload, 'uploadPendingPhoto');
      fillInAdd(editor, upload);
      button(element.querySelector('form')!, 'Add photo').click();
      // The control reads the file (a promise) before it sends it.
      await vi.waitFor(() => expect(success).toHaveBeenCalledWith('Photo added.'));
      fixture.detectChanges();

      expect(add).toHaveBeenCalledWith({
        event_id: LEATHER_NIGHT,
        alt_text: 'The bar from the door',
        caption: 'Doors at nine.',
        credit: 'Photo: the venue',
        consent_confirmed: true,
      });
      const newId = (await storedOrder(crud)).find((id) => id > 4)!;
      expect(sendPhoto).toHaveBeenCalledWith(TABLE, newId);
      expect(add.mock.invocationCallOrder[0]).toBeLessThan(sendPhoto.mock.invocationCallOrder[0]);
      expect((await firstValueFrom(photo.hasPhoto(TABLE, newId))).has_photo).toBe(true);

      // Last in the gallery, its words trimmed, and the form ready for another.
      expect(shownIds(element)).toEqual([1, 2, 3, 4, newId]);
      expect(text(row(element, newId).querySelector('.gallery-editor__alt'))).toBe(
        'The bar from the door',
      );
      expect(await storedOrder(crud)).toEqual([1, 2, 3, 4, newId]);
      expect(editor.addForm.getRawValue()).toEqual({
        altText: '',
        caption: '',
        credit: '',
        consent: false,
      });
      expect(upload.hasPendingFile).toBe(false);
      expect(success).toHaveBeenCalledWith('Photo added.');
      expect(text(element.querySelector('.gallery-editor__count'))).toBe('5 of 12 photos');
    });

    it('asks for the alt text, the consent box and the photo before sending anything', () => {
      const { fixture, editor, element, upload } = create();
      const add = vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'addGalleryPhoto');
      const send = () => {
        button(element.querySelector('form')!, 'Add photo').click();
        fixture.detectChanges();
        return text(element.querySelector('form .gallery-editor__error'));
      };
      editor.addForm.patchValue({ altText: '   ', consent: true });
      upload.pendingFile = PICTURE();
      expect(send()).toBe('Describe the photo for people who can’t see it.');

      editor.addForm.patchValue({ altText: 'The bar', consent: false });
      expect(send()).toBe('Confirm that everyone recognizable in the photo agreed to be shown.');

      editor.addForm.patchValue({ consent: true });
      upload.clearPendingFile();
      expect(send()).toBe('Choose the photo to add.');
      expect(add).not.toHaveBeenCalled();
    });

    it('says what the cap is when the gallery is full, and uploads nothing', async () => {
      const { fixture, editor, element, upload } = create(LEATHER_NIGHT, (_mocks, store) =>
        store.save(4),
      );
      const sendPhoto = vi.spyOn(upload, 'uploadPendingPhoto');
      expect(text(element.querySelector('.gallery-editor__count'))).toBe('4 of 4 photos');
      fillInAdd(editor, upload);
      button(element.querySelector('form')!, 'Add photo').click();
      await fixture.whenStable();
      fixture.detectChanges();
      expect(text(element.querySelector('form .gallery-editor__error'))).toBe(
        'This gallery is full: it can hold 4 photos. Remove one first, or raise the limit on ' +
          'the Event settings page.',
      );
      expect(sendPhoto).not.toHaveBeenCalled();
      expect(shownIds(element)).toEqual([1, 2, 3, 4]);
      // What was typed stays, for after a removal.
      expect(editor.addForm.controls.altText.value).toBe(' The bar from the door ');
      expect(upload.hasPendingFile).toBe(true);
    });

    it('takes the row back out when its photo doesn’t go up', async () => {
      const { fixture, editor, element, crud, upload } = create();
      vi.spyOn(upload, 'uploadPendingPhoto').mockReturnValue(
        throwError(() => new Error('Failed to load image')),
      );
      const deleteRow = vi.spyOn(crud, 'deleteItem');
      fillInAdd(editor, upload);
      button(element.querySelector('form')!, 'Add photo').click();
      await fixture.whenStable();
      fixture.detectChanges();

      expect(deleteRow).toHaveBeenCalledTimes(1);
      expect(deleteRow.mock.calls[0][0]).toBe(TABLE);
      expect(text(element.querySelector('form .gallery-editor__error'))).toBe(
        "The photo couldn't be uploaded, so nothing was added. Try a JPEG or PNG.",
      );
      expect(await storedOrder(crud)).toEqual([1, 2, 3, 4]);
      expect(shownIds(element)).toEqual([1, 2, 3, 4]);
    });

    it('holds the chosen photo back for the new row', () => {
      const { upload } = create();
      expect(upload.deferUpload).toBe(true);
      expect(upload.tableName).toBe(TABLE);
    });
  });

  it('says so when the gallery can’t be read', () => {
    const { element } = create(LEATHER_NIGHT, (mocks) =>
      vi
        .spyOn(mocks.crud, 'getFilteredTableRows')
        .mockReturnValue(throwError(() => new Error('offline'))),
    );
    expect(text(element.querySelector('.gallery-editor__error'))).toBe(
      "The gallery couldn't be loaded. Please try again.",
    );
  });

  it('leaves the count off when the cap can’t be read', () => {
    TestBed.configureTestingModule({
      providers: [
        provideNoopAnimations(),
        provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS),
        provideEventsAdminAccess(true),
      ],
    });
    vi.spyOn(TestBed.inject(EVENTS_ADMIN_ACCESS), 'getGallerySettings').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    const fixture = TestBed.createComponent(EventGalleryEditor);
    fixture.componentRef.setInput('eventId', LEATHER_NIGHT);
    fixture.detectChanges();
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('.gallery-editor__count')).toBeNull();
    expect(shownIds(fixture.nativeElement)).toEqual([1, 2, 3, 4]);
  });
});

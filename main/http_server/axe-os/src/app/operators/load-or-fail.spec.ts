import { fakeAsync, tick } from '@angular/core/testing';
import { NEVER, of, throwError } from 'rxjs';
import { ToastrService } from 'ngx-toastr';

import { loadOrFail } from './load-or-fail';

describe('loadOrFail', () => {
  let toastr: jasmine.SpyObj<ToastrService>;

  beforeEach(() => {
    toastr = jasmine.createSpyObj<ToastrService>('ToastrService', ['error']);
  });

  it('passes values through from a healthy source', () => {
    const seen: number[] = [];
    of(42).pipe(loadOrFail(toastr)).subscribe(v => seen.push(v));
    expect(seen).toEqual([42]);
    expect(toastr.error).not.toHaveBeenCalled();
  });

  it('completes with a toast when the source never answers', fakeAsync(() => {
    let completed = false;
    let errored = false;
    NEVER.pipe(loadOrFail(toastr, 15000)).subscribe({
      complete: () => (completed = true),
      error: () => (errored = true)
    });
    tick(15001);
    expect(completed).toBeTrue();
    expect(errored).toBeFalse();
    expect(toastr.error).toHaveBeenCalledTimes(1);
  }));

  it('completes with a toast when the source errors', () => {
    let completed = false;
    throwError(() => new Error('boom')).pipe(loadOrFail(toastr)).subscribe({
      complete: () => (completed = true)
    });
    expect(completed).toBeTrue();
    expect(toastr.error).toHaveBeenCalledTimes(1);
  });
});

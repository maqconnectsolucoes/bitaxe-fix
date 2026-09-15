import { EMPTY, MonoTypeOperatorFunction, TimeoutError, catchError, timeout } from 'rxjs';
import { ToastrService } from 'ngx-toastr';

/**
 * Guards the "load settings once" streams that feed a form behind
 * LoadingService.lockUIUntilComplete(): a device that never answers (or
 * errors) used to leave the UI locked on the spinner until a page reload.
 * Errors and silence are turned into a toast plus completion, so the caller's
 * lockUIUntilComplete() releases the UI.
 */
export function loadOrFail<T>(toastr: ToastrService, ms: number = 15000): MonoTypeOperatorFunction<T> {
  return source => source.pipe(
    timeout(ms),
    catchError(err => {
      const message = err instanceof TimeoutError
        ? 'Device did not respond.'
        : `Could not load settings. ${err?.message ?? err}`;
      toastr.error(message);
      return EMPTY;
    })
  );
}

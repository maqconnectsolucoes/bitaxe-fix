import { AbstractControl, ValidationErrors, ValidatorFn } from '@angular/forms';

/** One or more concatenated PEM certificates. Empty is allowed. */
export function pemCertificateValidator(): ValidatorFn {
  return (control: AbstractControl): ValidationErrors | null => {
    const value = control.value?.trim();
    if (!value) return null;

    const pemChainRegex =
      /^(?:-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----\s*)+$/;

    return pemChainRegex.test(value) ? null : { invalidCertificate: true };
  };
}

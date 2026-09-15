import { AbstractControl, ValidationErrors, ValidatorFn } from '@angular/forms';

/** Stratum V2 authority public key: base58, 40-52 characters. Empty is allowed. */
export function base58Validator(): ValidatorFn {
  return (control: AbstractControl): ValidationErrors | null => {
    const value = control.value?.trim();
    if (!value) return null;

    const base58Regex = /^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$/;
    if (!base58Regex.test(value)) {
      return { invalidBase58: true };
    }

    if (value.length < 40 || value.length > 52) {
      return { invalidBase58Length: true };
    }

    return null;
  };
}

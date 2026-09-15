import { AbstractControl, ValidationErrors } from '@angular/forms';

/** Swarm peer address: bare hostname, mDNS name (x.local) or dotted IPv4. */
export function addressValidator(control: AbstractControl): ValidationErrors | null {
  const value = control.value;
  if (!value) return null;
  const parts = value.split('.');
  switch (parts.length) {
    case 1: // Bare hostname (e.g. "bitaxe")
      return /^[a-zA-Z0-9-]+$/.test(parts[0]) ? null : { invalidAddress: true };
    case 2: // mDNS hostname (e.g. "bitaxe.local")
      if (parts[1].toLowerCase() === 'local' && /^[a-zA-Z0-9-]+$/.test(parts[0])) return null;
      break;
    case 4: // IP Address (e.g. "192.168.1.1")
      if (parts.every((part: string) => /^\d+$/.test(part) && Number(part) >= 0 && Number(part) <= 255)) return null;
      break;
  }
  return { invalidAddress: true };
}

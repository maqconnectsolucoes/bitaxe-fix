import { FormControl } from '@angular/forms';

import { addressValidator } from './address.validator';
import { base58Validator } from './base58.validator';
import { pemCertificateValidator } from './pem-certificate.validator';

const ctl = (value: any) => new FormControl(value);
const PEM = '-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----';

describe('addressValidator', () => {
  it('accepts a bare hostname', () => expect(addressValidator(ctl('bitaxe'))).toBeNull());
  it('accepts an mDNS name', () => expect(addressValidator(ctl('bitaxe.local'))).toBeNull());
  it('accepts a dotted IPv4', () => expect(addressValidator(ctl('192.168.1.1'))).toBeNull());
  it('accepts empty (required is a separate validator)', () => expect(addressValidator(ctl(''))).toBeNull());
  it('rejects an octet above 255', () => expect(addressValidator(ctl('256.1.1.1'))).toEqual({ invalidAddress: true }));
  it('rejects a two-label name that is not .local', () => expect(addressValidator(ctl('bitaxe.lan'))).toEqual({ invalidAddress: true }));
  it('rejects a hostname with spaces', () => expect(addressValidator(ctl('bit axe'))).toEqual({ invalidAddress: true }));
});

describe('base58Validator', () => {
  const validate = base58Validator();
  it('accepts a 44-char base58 key', () => expect(validate(ctl('1'.repeat(44)))).toBeNull());
  it('accepts empty', () => expect(validate(ctl(''))).toBeNull());
  it('rejects the ambiguous characters 0OIl', () => expect(validate(ctl('0OIl' + '1'.repeat(40)))).toEqual({ invalidBase58: true }));
  it('rejects a key that is too short', () => expect(validate(ctl('1'.repeat(39)))).toEqual({ invalidBase58Length: true }));
  it('rejects a key that is too long', () => expect(validate(ctl('1'.repeat(53)))).toEqual({ invalidBase58Length: true }));
});

describe('pemCertificateValidator', () => {
  const validate = pemCertificateValidator();
  it('accepts a single certificate', () => expect(validate(ctl(PEM))).toBeNull());
  it('accepts a certificate chain', () => expect(validate(ctl(`${PEM}\n${PEM}\n`))).toBeNull());
  it('accepts empty', () => expect(validate(ctl(''))).toBeNull());
  it('rejects garbage', () => expect(validate(ctl('not a cert'))).toEqual({ invalidCertificate: true }));
  it('rejects a truncated certificate', () => expect(validate(ctl('-----BEGIN CERTIFICATE-----\nMIIB'))).toEqual({ invalidCertificate: true }));
});

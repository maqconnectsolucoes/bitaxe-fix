import { FormControl } from '@angular/forms';

import { hostnameValidator } from './hostname.validator';

const ctl = (value: any) => new FormControl(value);

describe('hostnameValidator', () => {
  it('accepts a fully qualified domain', () => {
    // addressValidator (do swarm) rejeita isto: ele só conhece nome simples,
    // .local e IPv4. Um broker externo quase sempre tem domínio completo.
    expect(hostnameValidator(ctl('mqtt.exemplo.com.br'))).toBeNull();
  });
  it('accepts a bare hostname', () => expect(hostnameValidator(ctl('broker'))).toBeNull());
  it('accepts a dotted IPv4', () => expect(hostnameValidator(ctl('10.1.1.20'))).toBeNull());
  it('accepts empty (required is a separate validator)', () => expect(hostnameValidator(ctl(''))).toBeNull());

  it('rejects a URL scheme', () => {
    // O usuário cola "mqtts://host" com frequência; o cliente espera só o host.
    expect(hostnameValidator(ctl('mqtts://mqtt.exemplo.com'))).toEqual({ invalidHostname: true });
  });
  it('rejects an embedded port', () => expect(hostnameValidator(ctl('mqtt.exemplo.com:8883'))).toEqual({ invalidHostname: true }));
  it('rejects spaces', () => expect(hostnameValidator(ctl('mqtt exemplo'))).toEqual({ invalidHostname: true }));
  it('rejects a trailing dot-only label', () => expect(hostnameValidator(ctl('mqtt..com'))).toEqual({ invalidHostname: true }));
  it('rejects an octet above 255 in an IPv4-shaped value', () => expect(hostnameValidator(ctl('10.1.1.300'))).toEqual({ invalidHostname: true }));
  it('rejects a label longer than 63 characters', () => expect(hostnameValidator(ctl('a'.repeat(64) + '.com'))).toEqual({ invalidHostname: true }));
});

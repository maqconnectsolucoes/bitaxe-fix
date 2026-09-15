import { AbstractControl, ValidationErrors } from '@angular/forms';

/**
 * Hostname de um servidor externo: nome simples, domínio completo ou IPv4.
 *
 * Diferente do addressValidator do swarm, que só aceita nome simples, .local e
 * IPv4 — um broker fora da rede local quase sempre tem domínio completo. Aceita
 * vazio porque a obrigatoriedade é decidida por Validators.required à parte.
 */
export function hostnameValidator(control: AbstractControl): ValidationErrors | null {
  const value = control.value?.trim();
  if (!value) return null;

  const invalid = { invalidHostname: true };

  // Esquema e porta são erros comuns de colagem ("mqtts://host:8883"); a porta
  // tem campo próprio e o esquema não é aceito pelo cliente MQTT.
  if (/[:/\s]/.test(value)) return invalid;

  const labels = value.split('.');
  if (labels.some((label: string) => !/^[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$/.test(label))) {
    return invalid;
  }

  // Quatro grupos só de dígitos é um IPv4: valida os octetos em vez de aceitar
  // qualquer número, senão "10.1.1.300" passaria como se fosse um nome.
  if (labels.length === 4 && labels.every((label: string) => /^\d+$/.test(label))) {
    return labels.every((label: string) => Number(label) <= 255) ? null : invalid;
  }

  return null;
}

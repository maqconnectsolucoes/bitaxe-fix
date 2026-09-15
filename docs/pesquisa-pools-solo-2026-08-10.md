# Pools solo e rentabilidade com dois Bitaxe

Pesquisa de 10/08/2026, refeita com o **hashrate real lido no painel do relógio**:

| Aparelho | Hashrate | Eficiência medida (julho) | Potência |
|---|---|---|---|
| Dev 1 | 1,2 TH/s | 15,20 J/TH | ~18,2 W |
| Dev 2 | 1,1 TH/s | 18,17 J/TH | ~20,0 W |
| **Soma** | **2,3 TH/s** | | **~38 W** |

Hardware: dois Bitaxe Gamma (BM1370). Versões anteriores deste documento usaram 2,65 TH/s (derivado
das frequências de julho) e depois 2,00 TH/s (estimativa). Os números abaixo são os do painel.

Note que o Dev 2 gasta **mais** watts para entregar **menos** hashrate — o que confirma a ressalva do
fim deste documento: é silício pior, não desajuste.

> Nota de escopo: este documento é análise econômica do setup pessoal, não documentação do firmware.
> Está versionado a pedido, para que os números e as premissas sobrevivam à sessão em que foram
> levantados. Contém a tarifa de energia e as eficiências medidas dos aparelhos — nenhum segredo,
> mas é informação de uso doméstico, e este repositório é um fork de um projeto público.

## Dados da rede naquele dia

| Parâmetro | Valor | Fonte |
|---|---|---|
| Dificuldade | 127.479.855.693.691 (127,48 T) | CoinWarz, bloco 961.905 |
| Hashrate da rede | 937 EH/s | Hashrate Index, 03/08 |
| Preço BTC | ~US$ 65.000 | Yahoo Finance |
| Recompensa | 3,125 BTC + 0,025 de taxas | pós-halving 2024 |
| Hashprice | US$ 32,10 / PH/s / dia | Hashrate Index |
| USD/BRL | 5,1022 | Investing.com |

## A conta

```
Trabalho por bloco = D x 2^32 = 5,4752e23 hashes
Tempo medio        = 5,4752e23 / 2,30e12 = 2,3805e11 s = ~7.543 ANOS

Chance por dia = 86.400 / 2,3805e11 = 1 em 2.755.200  (0,0000363%)
Chance por ano = 1 - e^-0,000132565 = 1 em 7.543      (0,0133%)

Valor esperado = 0,000132565 x US$ 204.750 = US$ 27,15/ano = US$ 0,0743/dia
Conferencia por hashprice: 32,10 x 0,00230 = US$ 0,0738/dia  (bate com 1% de erro)
```

**Nenhuma pool solo muda essa chance.** Ela depende só de hashrate ÷ dificuldade. O que muda entre
pools é taxa, uptime e custódia — e a diferença de valor esperado entre 0% e 2% de taxa é de
**US$ 0,54 por ano**.

## Pools, ordenadas por valor esperado real

| # | Pool | Taxa | EV/dia | Observação |
|---|---|---|---|---|
| 1 | **Ocean (TIDES) + Lightning** | 0% | US$ 0,0743 | Não é solo — e por isso ganha. Sem mínimo via Lightning, então você **recebe de fato** |
| 2 | **public-pool.io** | 0% | US$ 0,0743 | Melhor solo puro. Open-source, self-hostável. Menor que a CKPool = mais risco de downtime |
| 3 | **ViaBTC solo** | 1% | US$ 0,0736 | Infra grande, mas custodial e com KYC |
| 4 | **solo.ckpool.org** | 2% | US$ 0,0728 | Config atual. 139 PH/s, 23.689 usuários, non-custodial, uptime comprovado |
| 5 | **solo100.org** | 2% | US$ 0,0728 | 10% ao achador + 88% dividido entre os 100 melhores shares (~US$ 1.790 cada). Redistribui variância, não cria valor. Aparece como "warming up", sem hashrate publicado |
| 6 | MySoloPool / SoloPool.org | 2% | US$ 0,0728 | Sem diferencial sobre a CKPool |

Downtime é a única coisa que realmente reduz o valor esperado. Taxa é ruído.

## US$ 1/dia: não existe em SHA-256

O mercado é arbitrado — o hashrate migra para onde paga mais e iguala tudo.

| Moeda | Em 2,30 TH/s |
|---|---|
| Bitcoin | US$ 0,074/dia |
| Bitcoin Cash | US$ 0,067/dia |
| Bitcoin SV | ~US$ 0,04-0,06/dia |
| DigiByte SHA-256 | não é mais PoW ativo |
| Merge mining (melhor caso) | US$ 0,089/dia |

Falta de **11x** (merge mining, melhor caso) a **13x** (bitcoin puro). zpool/NiceHash não ajudam:
o preço deles deriva do mesmo hashprice, com margem embutida.

## Balanço com energia

```
Energia:   0,917 kWh/dia x R$ 0,90 = R$ 0,825/dia
Receita:   US$ 0,0743 x 5,1022     = R$ 0,379/dia
Resultado: -R$ 0,45/dia  ->  -R$ 13,58/mes  ->  -R$ 163/ano
```

**R$ 2,18 de energia para cada R$ 1,00 de bitcoin produzido.**

Essa razão é o número que importa, e ela **não muda com escala**. As três versões deste documento
deram o mesmo resultado:

| Hashrate assumido | Perda/dia | Razão |
|---|---|---|
| 2,65 TH/s (frequências de julho) | -R$ 0,52 | R$ 2,19 : 1,00 |
| 2,00 TH/s (estimativa) | -R$ 0,39 | R$ 2,18 : 1,00 |
| **2,30 TH/s (painel, real)** | **-R$ 0,45** | **R$ 2,18 : 1,00** |

A perda absoluta acompanha o hashrate porque se gasta mais ou menos energia; a proporção não se move,
porque depende de J/TH contra tarifa. Foi por isso que a conclusão sobreviveu a duas correções do
número de entrada.

E escalar piora na mesma proporção: a margem por TH/s é negativa em **-US$ 0,038/TH/dia**. Os ~31
TH/s necessários para US$ 1,00/dia de receita — cerca de **27 Bitaxe** como os seus — queimariam
US$ 2,18/dia de energia.

- Eficiência de break-even na sua tarifa: **7,6 J/TH**. Seus aparelhos: 15,20 e 18,17 J/TH.
- O Antminer S23 (topo de 2026, ~9,5 J/TH) ainda ficaria 25% acima do break-even.
- Tarifa de break-even para o seu setup: **R$ 0,41/kWh**. Você paga R$ 0,90.

Esses dois números também são invariantes de escala — dependem só de eficiência contra tarifa.
São a razão de a conclusão não depender de acertar o hashrate.

Na tarifa residencial brasileira, nenhum hardware SHA-256 de 2026 fecha a conta. Não é limitação do
Bitaxe — é a tarifa.

## Uma ressalva ao relatório original, agora confirmada

O agente sugeriu que o Dev 2 estaria "mal calibrado" por consumir mais watts em frequência menor.
Isso não se sustentava contra a varredura de julho (corrente de core 14,8 A contra 12,3 A, ventoinha
a 60% contra 42% para o mesmo alvo térmico), e o painel confirma: **o Dev 2 entrega 1,1 TH/s gastando
~20 W, enquanto o Dev 1 entrega 1,2 TH/s com ~18 W.** Mais energia por menos trabalho, de forma
consistente. É silício pior, e os 18,17 J/TH são provavelmente o limite daquele chip — não há
calibração que resolva.

## Dados não confirmados

Hashrate total de public-pool.io, ViaBTC solo, MySoloPool e SoloPool.org; payouts mínimos exatos
dessas três; número preciso do BSV por TH/s; e qualquer estatística operacional da solo100.org.

As potências de ~18 W e ~20 W são derivadas (hashrate do painel × J/TH de julho), não medidas na
tomada. Se você medir o consumo real e ele divergir, o que muda é a razão R$ 2,18 : 1,00 — e essa é
a única linha do documento que importaria refazer.

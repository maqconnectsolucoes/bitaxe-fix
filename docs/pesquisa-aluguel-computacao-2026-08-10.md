# Viabilidade de gerar renda alugando capacidade computacional

**Caso:** residência no Brasil, 2 × Bitaxe Gamma (BM1370), 2,3 TH/s, ~38 W
**Tarifa:** R$ 0,90/kWh · **Câmbio:** USD/BRL 5,1022
**Linha de base:** receita R$ 0,38/dia, energia R$ 0,83/dia → **−R$ 0,45/dia** (R$ 2,18 de energia por R$ 1,00 produzido)
**Data da pesquisa:** 10 de agosto de 2026

---

## 1. Conclusão (leia só isto se for ler uma coisa só)

**Nenhum dos quatro caminhos fecha a conta na tarifa residencial brasileira de R$ 0,90/kWh com o hardware que você tem.** Não é questão de escolher a plataforma certa: é aritmética de eficiência energética.

O número que resolve tudo:

> **O Bitaxe Gamma consome 0,3965 kWh por TH por dia. O mercado paga R$ 0,164 por TH por dia. Logo, o preço máximo de energia em que essa máquina empata é R$ 0,41/kWh.**

Você paga R$ 0,90/kWh — 2,18× o ponto de empate. E esse fator de 2,18 é exatamente a razão que você já observou (R$ 2,18 de energia por R$ 1,00 produzido). Os números fecham entre si, o que dá confiança no resto da análise.

Consequências diretas:

- **Alugar hashrate não muda nada.** Confirmei em três fontes independentes no mesmo dia que alugar hashrate SHA-256 paga essencialmente o mesmo que minerar em pool. Você trocaria −R$ 0,45/dia por −R$ 0,44/dia. Não existe prêmio de aluguel a capturar.
- **Escalar não resolve.** Como o prejuízo é por TH, mais TH = mais prejuízo. Para chegar aos US$ 1,00/dia de *receita* você precisaria de ~31 TH/s (13,5× a sua frota), que consumiriam ~515 W e custariam **R$ 11,10/dia de energia** para produzir R$ 5,10/dia. A meta de US$ 1,00/dia é inalcançável com SHA-256 nessa tarifa em qualquer escala.
- **O caminho que chega mais perto sem gastar nada:** tarifa branca + desligar os mineradores no horário de ponta. Reduz o prejuízo de **−R$ 0,45 para ≈ −R$ 0,29/dia**. Continua prejuízo, mas é a única melhoria com capital zero. (Estimativa minha — ver §5, os multiplicadores de ponta não foram confirmados.)
- **O único caminho que aritmeticamente pode passar de US$ 1,00/dia** é comprar uma GPU classe RTX 4090 (R$ 11.000–16.000 só a placa, out. 2026) e alugá-la. Payback entre 3 e 8 anos dependendo de premissas que **não consegui confirmar** (ocupação real de host residencial). É uma aposta de capital, não uma monetização do que você já tem.
- **Não existe plataforma que pague por 38 W de capacidade ociosa sem GPU.** Confirmado: todo o mercado de aluguel de compute doméstico exige GPU NVIDIA, tipicamente série 30/40 com ≥12 GB de VRAM.

**A frase honesta:** isso não fecha em lugar nenhum na tarifa residencial brasileira com esse hardware. O Bitaxe Gamma é um bilhete de loteria e um objeto educativo, não um ativo de renda. Qualquer caminho para renda aqui passa por **comprar hardware novo** ou por **derrubar a tarifa de energia abaixo de R$ 0,41/kWh** — e não por trocar de plataforma.

---

## 2. Ranking por resultado financeiro real

Ordenado do melhor para o pior resultado diário, comparado à linha de base de **−R$ 0,45/dia**.

| # | Caminho | Resultado/dia | Capital | Confiança |
|---|---------|--------------|---------|-----------|
| 1 | **Comprar RTX 4090 + alugar** (Vast/Salad) | **+R$ 5 a +R$ 15/dia** operacional | R$ 15.000–20.000 | Baixa — ocupação não confirmada |
| 2 | **Comprar RTX 3090/4070 + alugar** | +R$ 0,9 a +R$ 1,7/dia operacional | R$ 6.000–10.000 | Média — nunca paga o hardware |
| 3 | **Solar 3 kWp** (energia dos Bitaxe zerada) | ≈ +R$ 0,30/dia operacional | R$ 15.000–16.500 | Alta nos custos, payback péssimo |
| 4 | **Tarifa branca + desligar na ponta** | **≈ −R$ 0,29/dia** | **R$ 0** | Média — multiplicadores estimados |
| 5 | Tarifa branca sozinha (24/7) | ≈ −R$ 0,40/dia | R$ 0 | Média |
| 6 | **Linha de base (solo hoje)** | **−R$ 0,45/dia** | — | Confirmada |
| 7 | NiceHash / MRR SHA256-AsicBoost | −R$ 0,44/dia | R$ 0 | **Alta — 3 fontes** |
| 8 | MiningRigRentals mercado sha256 legado | ≈ −R$ 0,82/dia | R$ 0 | Alta — ocupação 1,65% medida |
| 9 | Akash / io.net / Render | ≈ −R$ 0,83/dia (não aceitam o hardware) | R$ 0 | Alta |

Observe que os itens 1, 2 e 3 são **resultados operacionais** — não descontam a depreciação nem o custo de oportunidade do capital. Com o capital incluído, nenhum dos três tem payback aceitável para este caso, como detalho abaixo.

---

## 3. Pergunta 1 — Alugar hashrate SHA-256

### 3.1 Quanto o mercado paga hoje (dado confirmado)

Três fontes independentes, coletadas em **10 de agosto de 2026**, convergem:

| Fonte | Preço | Data | Equivalente USD/PH/dia |
|-------|-------|------|------------------------|
| Luxor Hashrate Index (hashprice) | US$ 32,10 /PH/dia | 3 ago 2026 | **US$ 32,10** |
| NiceHash — order book SHA256ASICBOOST (API pública) | 0,52110000 BTC/EH/dia | 10 ago 2026 | **US$ 33,29** |
| MiningRigRentals — mercado sha256ab, último preço | 0,00054500 BTC/PH/dia | 10 ago 2026 | **US$ 34,82** |

Conversão feita com **BTC = US$ 63.888,98** (minerstat, 10 ago 2026 22:40) e USD/BRL 5,1022.

Dados de contexto confirmados no mesmo dia:
- Hashrate da rede Bitcoin: ~1.003 EH/s (7 ago 2026, via CoinWarz/minerstat); dificuldade 126,23 T.
- Luxor: hashprice de US$ 32,10 representou queda de 1,3% sobre os US$ 32,53 da semana anterior. O mercado futuro precificava média de US$ 30,83 (0,00048 BTC) para os 6 meses seguintes — ou seja, **a expectativa do próprio mercado é de queda**, não de alta.

**Por TH/dia isso dá US$ 0,0321 a US$ 0,0348 = R$ 0,164 a R$ 0,178.**

Para os seus 2,3 TH/s: **R$ 0,377 a R$ 0,409/dia bruto**. Sua receita solo declarada de R$ 0,38/dia bate exatamente com o meio dessa faixa — sua contabilidade está correta.

### 3.2 A conclusão desconfortável: aluguel ≈ mineração

O prêmio que você esperaria de "alugar em vez de minerar" **não existe** no SHA-256. Os três preços acima estão dentro de 8% um do outro. Isso faz sentido estrutural: o comprador de hashrate está comprando a mesma expectativa de bloco que você teria minerando, então o preço de equilíbrio é o hashprice mais um spread pequeno.

**NiceHash (líquido, funciona tecnicamente):**
- Order book com **1.250 ordens ativas** ao preço uniforme de 0,5211 BTC/EH/dia, mercado com ~15,6 EH/s. É líquido — sua hashrate seria absorvida.
- Taxa do vendedor: **2% sobre os ganhos** (NiceHash, consultado ago 2026).
- Saque mínimo: 0,00001 BTC (1.000 sat) para carteira interna; **0,001 BTC para carteira externa**. A 2,3 TH/s você produz ~1,2 × 10⁻⁶ BTC/dia → **levaria ~830 dias para atingir o mínimo de saque externo**. Via Lightning a taxa é zero e o limite interno é baixo, o que torna isso administrável, mas é um atrito real.
- Dificuldade mínima do pool: NiceHash já elevou a dificuldade mínima do SHA-256 no passado. A 2,3 TH/s, com dificuldade 65.536, você submeteria uma share a cada ~122 s — tecnicamente viável. **Não encontrei confirmação do valor de dificuldade mínima vigente em 2026.**

Resultado NiceHash: R$ 0,391/dia bruto − 2% = **R$ 0,383/dia**, contra R$ 0,83 de energia = **−R$ 0,447/dia**. Idêntico à linha de base.

**MiningRigRentals (não funciona para 2,3 TH/s):**

Aqui os dados são muito mais duros. Capturei os dois mercados SHA-256 da MRR em 10 ago 2026:

*Mercado sha256ab (AsicBoost — onde está o volume real):*
- Disponível: **1.381 rigs, 37,02 EH/s**
- Alugado agora: **840 rigs, 622,58 PH/s**
- **Ocupação por hashrate: 1,65%** · Ocupação por número de rigs: 37,8%
- Último preço 0,000545 BTC/PH; sugerido 0,0005367; faixa 30d 0,00051390–0,00064333
- 31.589 aluguéis em 30 dias, 832 locatários únicos

*Mercado sha256 (legado):*
- Disponível: **187 rigs, 2,15 EH/s** · Alugado: **81 rigs, 14,45 PH/s**
- **Ocupação por hashrate: 0,67%**
- Último preço 0,001199 BTC/PH (= US$ 76,60/PH/dia, ~2,3× o hashprice), sugerido 0,0010776, faixa 30d 0,00066–0,004131
- Hash ao vivo: apenas 42,32 PH — **mercado moribundo**

**Existe mercado para 2,3 TH/s? Não.** Duas evidências:

1. **Tamanho típico da unidade negociada.** Os 840 rigs alugados somam 622,58 PH/s → média de **741 TH/s por rig alugado**. Seu conjunto inteiro é **0,31% de um rig médio alugado** — cerca de 320× menor. Você não é um participante pequeno desse mercado; você está três ordens de grandeza abaixo dele.

2. **A regra de preço mínimo.** Segundo o helpcenter da MRR (capturado via busca, **não consegui verificar na página ao vivo**), o preço mínimo de um aluguel de 3 horas é de **100 satoshi**. Fazendo a conta: 2,3 TH/s a 0,000545 BTC/PH/dia rendem 125 sat/dia, ou **15,7 sat em 3 horas**. Para atingir o mínimo de 100 sat você teria que precificar seu rig a **6,4× o preço de mercado**. Nesse preço ninguém aluga — e a MRR ainda exige 20 minutos consecutivos online antes de listar.

**Ocupação real — o número que decide.** É aqui que o material promocional mente por omissão. As plataformas publicam o preço por PH; ninguém publica que **98,35% da hashrate listada na MRR não está alugada neste instante**. Se você conseguisse contornar o preço mínimo e listasse a 0,000545 BTC/PH, sua receita esperada seria R$ 0,39/dia × 1,65% ≈ **R$ 0,006/dia**, contra R$ 0,83 de energia. Ou seja: **listar na MRR é estritamente pior do que minerar em pool**, porque você paga a energia integral e recebe quase nada.

Uma ressalva de honestidade: a ocupação por *contagem de rigs* é 37,8%, muito maior que a por hashrate. A diferença vem de a oferta ser dominada por poucos listings gigantescos (média de 26,8 PH por rig disponível vs. 0,74 PH por rig alugado). **Qual das duas métricas se aplicaria a um rig de 2,3 TH/s eu não sei** — não achei dado de ocupação segmentado por tamanho. Mas as duas leituras levam ao mesmo lugar: mesmo a 37,8% de ocupação e ao preço-prêmio do mercado legado, a receita seria R$ 0,90/dia × 0,378 = R$ 0,34/dia, ainda abaixo dos R$ 0,83 de energia.

### 3.3 Taxas e mínimos (confirmado)

| Plataforma | Taxa | Mínimo de saque |
|---|---|---|
| NiceHash | 2% dos ganhos do minerador | 0,00001 BTC interno / 0,001 BTC externo |
| MiningRigRentals | 3% sobre transações | Não confirmado (autopayout configurável) |

---

## 4. Pergunta 2 — Alugar GPU

Você não tem GPU. Tudo aqui envolve compra, então a pergunta correta não é "quanto rende", é "quanto tempo leva para devolver o capital".

### 4.1 Preços reais de mercado (dado confirmado, 10 ago 2026)

Preços **pagos pelo cliente** na Vast.ai, via ComputePrices.com, atualização de 10 ago 2026:

| GPU | US$/h (cliente) | TDP | US$/h que o host recebe (≈ ÷1,25) |
|---|---|---|---|
| RTX 4060 / 4060 Ti | 0,063 | 115 W | ~0,050 |
| RTX 3070 | 0,078 | 220 W | ~0,062 |
| RTX 3080 | 0,082 | 320 W | ~0,066 |
| RTX 4070 Ti | 0,090 | 285 W | ~0,072 |
| RTX 4070 | 0,096 | 200 W | ~0,077 |
| RTX 3090 | 0,151 | 350 W | ~0,121 |
| RTX 3090 Ti | 0,176 | 450 W | ~0,141 |

RTX 4090 não aparece na tabela da Vast. Em outra fonte (ComputePrices, mesma data), a RTX 4090 vai de **US$ 0,160/h (Salad Cloud, o mais barato)** até uma mediana de **~US$ 0,456/h** entre 6 provedores. Uma fonte de início de 2026 citava US$ 0,34/h na Vast.

**Sobre o desconto de 25%:** a própria Vast.ai declara que "as taxas ao vivo são tipicamente 25% acima dos ganhos do host". A Vast **removeu a taxa direta de hosting em junho de 2024**, substituindo-a por uma sobretaxa interna aplicada ao preço do cliente. Ou seja, o host define e recebe o preço que definiu, mas o cliente vê ~25% a mais. Usei ÷1,25 como aproximação do que o host efetivamente embolsa em relação ao preço de tabela publicado.

### 4.2 Ocupação real — o número mais difícil, e o mais importante

Aqui está o contraditório que você pediu. Separei o que é marketing do que é relato:

**Material da plataforma (viés otimista, use com desconfiança):**
- Vast.ai: "um rig de quatro RTX 5090 a **80% de utilização** gera US$ 700–1.400/mês". Note a premissa: 80%.
- Salad: "seu PC pode ganhar **até** US$ 180 por mês" com 3090/3090 Ti/4090. "Até" fazendo todo o trabalho na frase.

**Relatos e dados de terceiros (o que realmente importa):**
- **Akash Network, Q1 2026 (Messari — dado auditado, não marketing):** utilização de GPU de **33,7%**, estável vs. Q4 2025. Uso médio caiu 57,4% no trimestre, para **84 GPUs**. Receita de leases caiu **45% no trimestre**. Provedores ativos em mínima histórica: **58**. Análise associada: "o marketplace ainda tem muito mais oferta do que demanda pagante na maior parte do tempo".
- **Relato de host individual (dev.to, RTX 3060 na Vast.ai):** 77 horas alugadas em 7 dias = **45,8% de ocupação na primeira semana**, US$ 11,56 brutos. Mas o próprio autor corrige: "um estado estacionário mais realista: **US$ 50–130/mês** dependendo da demanda", e a utilização é inconsistente (variou de 0 a 22 h/dia).
  **Aviso importante:** esse relato diz ter cobrado US$ 0,15/h por uma RTX 3060. Isso é **acima do preço da RTX 3090** na Vast hoje (US$ 0,151/h) e mais que o dobro da RTX 4060 (US$ 0,063/h). O número é implausível para as condições de 2026 e provavelmente é de um período de demanda muito diferente, ou promocional. **Não use esse relato para dimensionar receita.** Uso-o só como evidência de que a ocupação é irregular.
- **Revisões independentes:** "utilização real fica tipicamente em 50–70%"; e o contraponto: "sua placa não ganha nada nas horas em que ninguém aluga, e placas de consumo sem score de confiabilidade de datacenter frequentemente ficam paradas enquanto os compradores escolhem hosts mais confiáveis".
- **Sobre plataformas de compute ocioso em geral:** "marketplaces ficam com 15–25% dos ganhos brutos"; "a US$ 0,08/kWh dá margem confortável, **a US$ 0,30/kWh provavelmente não é lucrativo**". Sua tarifa é **US$ 0,1764/kWh** — no meio dessa faixa, tendendo ao lado ruim.

**Minha estimativa de trabalho:** 40–55% para um host residencial não-verificado no Brasil. É estimativa, não dado. Uso 50% nos cálculos abaixo e mostro também o ponto de equilíbrio.

### 4.3 Exigências que atrapalham quem é doméstico (confirmado)

- **Vast.ai:** "planeje oferecer **100% de uptime** durante o período de aluguel" e "espere que a GPU seja usada **próximo da capacidade máxima**". Clientes precisam de **portas abertas** para conectar direto à máquina — o que exige IP público e port forwarding, não CGNAT (comum em banda larga residencial brasileira). A verificação é automatizada e aleatória; **verificação manual só para datacenters e máquinas de ponta**. Máquinas que testam rápido: US$ 10 mil+ (H100/A100 em ~1 dia), sistemas 8×4090 ou 4×A6000 em menos de uma semana. Uma máquina de 1 GPU de consumo não tem prioridade nenhuma. Status de **datacenter exige número mínimo de servidores e certificação de terceiro (ex.: ISO 27001)** — inatingível em casa.
  - Recomendação comunitária (não oficial): **500 Mbps up/down** e 90%+ de uptime para ser "Verified". Não confirmei isso na documentação oficial — a doc da Vast **não publica mínimos de banda**.
- **Salad:** mínimo de **15 Mbps up e down**; recomendado Ethernet com ≥50 Mbps down / 10 Mbps up. Containers exigem **≥8 GB de RAM e 70 GB livres**. Trabalhos mais lucrativos vão para **NVIDIA série 30/40 com ≥12 GB de VRAM**. Saque mínimo tipicamente **US$ 5** (e é preciso ter um pouco mais que o valor exato). Pagamento em cartão pré-pago Visa, Venmo (só EUA), PayPal ou gift cards — **a disponibilidade dessas formas de saque no Brasil eu não confirmei**, e isso é um risco material.
- **Penalidades por queda:** não achei uma tabela de multas explícita. O mecanismo é indireto e pior: o *reliability score* cai, você desce no ranking, e sua máquina simplesmente para de ser alugada.

**Custo de oportunidade não financeiro:** enquanto a GPU está alugada, você não pode usá-la. Foi a queixa nº 1 do relato de host que li.

### 4.4 A conta, com a sua tarifa

R$ 0,90/kWh ÷ 5,1022 = **US$ 0,17640/kWh**.

Premissas minhas (estimativa): overhead do PC hospedeiro 80–100 W sob carga, 70–90 W ocioso; a máquina fica ligada 24 h.

**RTX 3090** (350 W + 90 W = 440 W carga; 90 W ocioso; host recebe US$ 0,121/h)
- Custo de energia sob carga: US$ 0,0776/h → margem quando alugada: **US$ 0,043/h**
- Custo ocioso: US$ 0,0159/h
- **Ocupação de equilíbrio: 27%**
- A 50% de ocupação: receita US$ 1,45/dia − energia US$ 1,12/dia = **+US$ 0,33/dia = R$ 1,68/dia**
- A 100% de ocupação: **+US$ 1,04/dia**. Ou seja, você só bateria a meta de US$ 1,00/dia com a GPU **alugada 24 h por dia, todos os dias**.
- Capital: RTX 3090 **não é mais vendida nova no varejo brasileiro** (busca no Zoom em 10 ago 2026 não retornou nenhum estoque de 3090; a 3080 aparece a R$ 8.703,64). Preço de 3090 usada no Brasil **não consegui confirmar**. Se fosse R$ 5.000 + PC R$ 3.000 = R$ 8.000 → **payback ≈ 15,8 anos** numa placa de 2020.

**RTX 4070** (200 W + 80 W = 280 W carga; 70 W ocioso; host recebe US$ 0,077/h)
- Margem alugada US$ 0,0276/h; custo ocioso US$ 0,0124/h → **ocupação de equilíbrio: 31%**
- A 50%: **+US$ 0,18/dia = R$ 0,93/dia**. Nunca paga a placa.

**RTX 4090** (450 W + 100 W = 550 W carga; host recebe US$ 0,128 a US$ 0,365/h dependendo de qual preço se acredita)
- Energia sob carga: US$ 0,097/h
- **Cenário pessimista** (host recebe US$ 0,20/h) a 50%: receita US$ 2,40 − energia US$ 1,38 = **+US$ 1,02/dia = R$ 5,22/dia**
- **Cenário otimista** (host recebe US$ 0,365/h) a 50%: receita US$ 4,38 − energia US$ 1,38 = **+US$ 3,00/dia = R$ 15,32/dia**
- Capital: **RTX 4090 entre R$ 11.000 e R$ 16.000** no Brasil (a partir de R$ 14.729 em uma cotação; faixa citada R$ 11.000–16.000, dados de 2026 via Adrenaline/Zoom) + PC ~R$ 4.000 → **R$ 15.000–20.000**
- **Payback: 2,7 anos (otimista) a 7,9 anos (pessimista)**, ignorando depreciação

**Este é o único caminho que aritmeticamente passa de US$ 1,00/dia.** E mesmo ele depende de uma ocupação de 50% que eu **não consegui confirmar** para host residencial brasileiro não-verificado, competindo com hosts americanos e europeus que pagam US$ 0,05–0,10/kWh — um terço do seu custo. Você entra nesse mercado com a estrutura de custo mais alta e o score de confiabilidade mais baixo.

Contexto que reforça o ceticismo: a receita anualizada de todo o setor DePIN de compute era de **US$ 180–220 milhões no Q1 2026**, com io.net em ~US$ 12,5 milhões anualizados (abaixo do que a trajetória 2024–2025 sugeria) e Akash em ~**US$ 1 milhão** anualizado. Não é um mercado com demanda sobrando esperando a sua placa.

### 4.5 Alguma plataforma paga por 38 W ociosos? Não.

Confirmado, e a resposta é um não sem asteriscos:
- Salad, Vast.ai, io.net e Akash **exigem GPU NVIDIA**; os trabalhos que pagam vão para série 30/40 com ≥12 GB de VRAM.
- Salad ainda exige 8 GB de RAM e 70 GB de disco livres no host.
- Os Bitaxe são ASICs SHA-256 — fisicamente incapazes de rodar qualquer workload de IA, render ou CPU. Não há caminho técnico.
- Plataformas de CPU ocioso (Golem, iExec, MQL5) existem, mas exigem CPU x86 e, mesmo assim, "pode levar meses só para atingir o valor mínimo de saque".

---

## 5. Pergunta 3 — Energia solar

### 5.1 Quanto seria preciso

Geração típica no Brasil: **4,2 a 5,8 horas de sol pleno (HSP)** por dia, irradiação de 4,5–6,5 kWh/m²/dia, com *performance ratio* de ~0,80. Isso dá **~4,0 kWh por kWp por dia** como número de trabalho realista.

| Cenário | Consumo | kWp necessários |
|---|---|---|
| **Bitaxe atual (38 W)** | 0,912 kWh/dia · 27,4 kWh/mês | **0,23 kWp** |
| **GPU 350 W** | 8,4 kWh/dia · 252 kWh/mês | **2,1 kWp** |

### 5.2 Custos no Brasil em 2026 (dado confirmado)

Preço médio nacional por Wp: **R$ 2,45 no 1º trimestre de 2026**. Mas isso é média incluindo sistemas grandes. Para residencial, por porte (Solarprime, **14 de julho de 2026**):

| Porte | R$/kWp | Investimento total |
|---|---|---|
| **3 kWp** | R$ 5.000–5.500 | **R$ 15.000–16.500** |
| 5 kWp | R$ 4.200–5.000 | R$ 21.000–25.000 |
| 8 kWp | R$ 3.700–4.500 | R$ 29.600–36.000 |
| 10 kWp | R$ 3.500–4.200 | R$ 35.000–42.000 |

Você estava certo ao antecipar que sistema pequeno tem custo por Wp muito pior: **R$ 5,00–5,50/Wp a 3 kWp contra R$ 3,50/Wp a 10 kWp** — 50% mais caro por watt. E abaixo de 3 kWp a curva piora ainda mais, porque projeto, ART, mão de obra e homologação são custos fixos.

**Um sistema de 0,23 kWp para os Bitaxe não existe como produto instalado e homologado.** Não achei nenhum fornecedor oferecendo grid-tie residencial abaixo de ~1,5 kWp.

### 5.3 Regras de compensação vigentes — Lei 14.300

**O que mudou e o que vale em 2026 (confirmado):**
- A cobrança do **Fio B** sobre energia injetada chegou a **60% em 2026**. O cronograma da Lei 14.300/2022: 45% em 2025, **60% em 2026**, 75% em 2027, 90% em 2028; a partir de 2029 vale a regra do art. 17 e a ANEEL define um novo modelo com base em estudos de custo-benefício.
- **A cobrança não atinge a energia gerada e consumida no mesmo instante** — só a que é injetada na rede e compensada depois.

**Isso é péssimo especificamente para o seu caso.** Os Bitaxe rodam 24 h/dia, mas o sol só gera ~5 h/dia. Cerca de 80% da energia que os alimentaria à noite teria que ser injetada de dia e compensada depois — exatamente a parcela que paga o Fio B a 60%.

**E há um obstáculo pior, que geralmente ninguém menciona:** o **custo de disponibilidade**. Para ligação **monofásica**, a distribuidora cobra um mínimo equivalente a **30 kWh/mês**, e os créditos solares **não abatem esse mínimo** — só o excedente. O consumo dos seus Bitaxe é de **27,4 kWh/mês**, ou seja, **inteiramente dentro desse piso**. Se a sua casa consumisse apenas os Bitaxe, o solar não economizaria absolutamente nada. Na prática sua casa consome mais, então os Bitaxe são carga marginal e o solar os abate na margem — mas o piso de 30 kWh continua sendo dinheiro que o solar nunca recupera.

### 5.4 Payback

**Caso Bitaxe (38 W):** economia máxima possível = 0,912 kWh/dia × R$ 0,90 = **R$ 0,82/dia = R$ 300/ano**. Descontando o Fio B a 60% sobre a parcela injetada, cai para ~R$ 250–270/ano.

O menor sistema que dá para instalar de fato (3 kWp, R$ 15.000–16.500) custaria **≈ 60 anos** para se pagar se o objetivo fosse só alimentar os Bitaxe. É a pior relação da lista inteira. Mesmo um sistema hipotético de R$ 3.000 daria 11 anos.

**Caso GPU (350 W / 8,4 kWh/dia):** economia = R$ 7,56/dia = R$ 2.760/ano, menos ~17% de Fio B ≈ **R$ 2.290/ano**. Sistema de 3 kWp a R$ 15.750 → **payback ≈ 6,9 anos**, coerente com a faixa publicada.

**Faixas publicadas de payback residencial em 2026 (para comparação):** as fontes divergem bastante — de "2 a 4 anos" a "4 a 7 anos", com "3,5 anos" como média citada. As fontes mais otimistas são, previsivelmente, de empresas que vendem sistemas solares. Uso 4–7 anos como faixa honesta. Vida útil 25–30 anos com garantia de 80% da potência aos 25.

**Veredito:** solar faz sentido para uma casa que já consome 250+ kWh/mês. **Não faz sentido nenhum como forma de viabilizar 38 W de mineração** — você gastaria R$ 15.000 para transformar um prejuízo de R$ 0,45/dia em um lucro de R$ 0,30/dia, o que devolve o capital em ~137 anos. Se você já pretende instalar solar por outros motivos, aí sim os Bitaxe passam a operar com energia quase de graça e ficam ligeiramente positivos. Mas aí o solar é a decisão, e o Bitaxe é o detalhe.

---

## 6. Pergunta 4 — Tarifas alternativas

### 6.1 Como funciona a tarifa branca (confirmado)

Modalidade opcional do Grupo B (baixa tensão), disponível desde 1º de janeiro de 2018, para consumidores residenciais, rurais e comerciais. **Excluídos:** baixa renda com subsídio e iluminação pública. Três postos tarifários em dias úteis:

- **Ponta:** normalmente entre 18h e 21h (na Enel SP, 17h30–20h30) — mais cara
- **Intermediária:** a hora anterior e a posterior à ponta (Enel SP: 16h30–17h30 e 20h30–21h30)
- **Fora de ponta:** demais horas — mais barata

**Fins de semana e feriados nacionais: sempre fora de ponta.** As janelas variam por distribuidora.

Adesão: solicitar à distribuidora, que tem 30 dias para trocar o medidor; há 30 dias de janela para voltar à convencional.

### 6.2 Quanto realmente muda (dado confirmado + estimativa minha)

**Confirmado:**
- No fora de ponta, fins de semana e feriados, a energia é **15% mais barata que a convencional**.
- Estimativa da **ANEEL: redução média de 5% na fatura** para quem migrou; em alguns casos chega a 15%.
- A ANEEL não publica os multiplicadores de ponta/intermediária de forma consolidada; os valores variam por distribuidora e por Revisão Tarifária Periódica.

**Não confirmado (e importante):** encontrei referência a uma proposta de estrutura em que a **ponta equivale a 5× o fora de ponta e a intermediária a 3×**, mas isso parece ser da **nova proposta regulatória em consulta pública**, não da tarifa branca vigente. Os valores concretos em R$/kWh por distribuidora eu **não consegui obter** — as páginas da Cemig, CPFL, Enel e ANEEL descrevem a estrutura mas não publicam a tabela.

**A conta, com estimativa explícita.** Uma carga constante 24/7 distribui-se assim ao longo da semana:

| Posto | Horas/semana | Participação |
|---|---|---|
| Ponta (3 h × 5 dias) | 15 h | **8,93%** |
| Intermediária (2 h × 5 dias) | 10 h | **5,95%** |
| Fora de ponta | 143 h | **85,12%** |

Com fora de ponta a R$ 0,765 (−15%, confirmado) e **minha estimativa** de ponta ≈ 2× e intermediária ≈ 1,35× o fora de ponta:

**Cenário A — tarifa branca, mineradores 24/7:**
Tarifa efetiva ≈ **R$ 0,849/kWh** (−5,6% — coerente com a estimativa da ANEEL de 5%).
Energia: R$ 0,774/dia. Receita R$ 0,391/dia. **Resultado: −R$ 0,38/dia.**
Melhora de ~R$ 0,07/dia sobre a linha de base. Marginal.

**Cenário B — tarifa branca + desligar os Bitaxe das 18h às 21h em dias úteis:**
- Receita cai 8,93%: R$ 0,391 → **R$ 0,356/dia**
- Energia: 0,830 kWh/dia à tarifa efetiva de R$ 0,782 → **R$ 0,650/dia**
- **Resultado: −R$ 0,29/dia** · razão energia/receita cai de 2,18:1 para **1,83:1**

**Este é o melhor resultado de capital zero de toda a pesquisa.** Reduz o prejuízo em 35%. Continua sendo prejuízo.

### 6.3 O que muda em 2026 (confirmado)

A área técnica da ANEEL propôs **migração automática para a tarifa branca** de consumidores de baixa tensão com consumo **≥ 1 MWh/mês** (~33,3 kWh/dia), começando em 2026 — cerca de **2,5 milhões de unidades** no primeiro ano. A proposta esteve em consulta pública até **março de 2026**. Projeção associada: quem não adaptar o consumo pode ver **aumento de 10–15%** na conta.

Você provavelmente está **abaixo** de 1 MWh/mês, então a migração compulsória não deve atingi-lo — a adesão continuaria voluntária.

Reajuste médio projetado para 2026: **+8%**, puxado pela CDE, que ultrapassou R$ 52 bilhões. Ou seja, **a tendência da sua tarifa é subir**, o que piora todos os cenários acima ao longo do tempo.

### 6.4 Outras modalidades

- **Tarifa social / baixa renda:** expressamente **excluída** da tarifa branca; e você precisaria estar no CadÚnico. Não se aplica.
- **Mercado livre de energia para residências:** a abertura para o Grupo B está em discussão regulatória há anos. **Não confirmei nenhuma modalidade residencial disponível em agosto de 2026** que permita comprar energia abaixo da tarifa regulada.
- **Bandeiras tarifárias:** variam mês a mês (bandeira amarela citada em maio/2026). Não são uma modalidade escolhível.

**Nenhuma modalidade tarifária residencial brasileira chega perto de R$ 0,41/kWh**, que é o que seria preciso para a mineração empatar. O melhor que a tarifa branca oferece — R$ 0,765/kWh no fora de ponta — ainda é **86% acima** do ponto de equilíbrio.

---

## 7. Síntese: por que nada disso funciona

Todos os quatro caminhos batem no mesmo muro por razões diferentes:

1. **Aluguel de hashrate** falha porque o preço de aluguel ≈ o preço de minerar. Não há arbitragem. E a 2,3 TH/s você está 320× abaixo da unidade mínima negociada.
2. **Aluguel de GPU** falha porque exige comprar hardware que custa 30–50× o valor do seu equipamento atual, com payback de 3 a 16 anos, competindo contra hosts que pagam um terço da sua energia.
3. **Solar** falha porque o custo por Wp de um sistema minúsculo é péssimo, o custo de disponibilidade de 30 kWh/mês engole todo o consumo dos Bitaxe, e o Fio B a 60% penaliza justamente o padrão noturno de consumo de um minerador.
4. **Tarifa branca** falha porque uma carga 24/7 não tem como fugir do horário de ponta sem abrir mão de receita, e o desconto de fora de ponta (15%) é uma fração do que seria preciso (54%).

O denominador comum: **R$ 0,41/kWh**. Enquanto sua energia custar mais que isso, cada TH/s que você liga destrói valor, e nenhuma plataforma, contrato ou modalidade tarifária conserta isso.

**A recomendação honesta:** mantenha os dois Bitaxe minerando solo pelo que eles realmente são — um bilhete de loteria de R$ 0,45/dia e um objeto de aprendizado sobre firmware, Stratum e ASICs. R$ 13,50/mês é um hobby barato. Não é, e não vai virar, uma fonte de renda. Se o objetivo for renda, o caminho é outro projeto, não outra plataforma para este hardware.

---

## 8. Dados não confirmados

Itens que ficaram em aberto, para você não tomar decisão em cima deles:

**Aluguel de hashrate**
- **Regra de preço mínimo da MRR (100 satoshi por aluguel de 3 h):** apareceu num resumo de busca do helpcenter da MRR, mas **não consegui verificar na página ao vivo**. As páginas de FAQ e helpcenter que consultei não trazem o número. Como esse item é decisivo para saber se um rig de 2,3 TH/s é sequer listável, vale confirmar direto na plataforma.
- **Mínimo de saque da MiningRigRentals:** não encontrado. Só confirmei que há autopayout configurável e retenção de 12 h pós-aluguel.
- **Dificuldade mínima do pool SHA-256 da NiceHash em 2026:** não encontrada. Há registro histórico de elevação, e a 2,3 TH/s existe risco teórico de shares rejeitadas.
- **Ocupação da MRR segmentada por tamanho de rig:** não existe publicamente. As duas métricas que calculei (1,65% por hashrate, 37,8% por contagem) divergem em 23×, e não sei qual se aplicaria a um rig de 2,3 TH/s.
- **Valor "paying" da API simplemultialgo da NiceHash:** obtive o número bruto (5,379×10⁻¹¹ para SHA256; 4,972×10⁻¹¹ para SHA256ASICBOOST) mas **não consegui reconciliar a unidade** — todas as interpretações plausíveis divergiam do hashprice por fatores de 10 ou 10⁴. Descartei esse número e usei o order book (0,5211 BTC/EH/dia), que bate com o hashprice da Luxor dentro de 4% e é internamente consistente.

**GPU**
- **Ocupação real de host residencial brasileiro não-verificado.** Este é o número que decide o caminho nº 1 do ranking, e ele não existe publicamente. Nenhuma plataforma publica; os relatos que achei são de primeira semana ou promocionais. Meus 50% são estimativa.
- **Preço da RTX 3090 usada no Brasil.** Mercado Livre e OLX retornaram HTTP 403; o Zoom não mostrou estoque de 3090. Sei que a 3080 nova está a R$ 8.703,64 e que a 3060 12 GB nova gira em R$ 1.800–2.400 (média R$ 2.149; mínimo histórico R$ 1.443,19 em 20/09/2025; máximo R$ 2.399,99 em 28/03/2026).
- **Preço exato da RTX 4090 hoje.** Tenho a faixa R$ 11.000–16.000 e uma cotação "a partir de R$ 14.729", mas de fontes de 2026 sem data precisa.
- **Percentual exato retido pela Vast.ai.** A empresa afirma ter removido a taxa de hosting em junho de 2024 e diz que os preços ao vivo são "tipicamente 25% acima dos ganhos do host". Não há número oficial publicado. Meu ÷1,25 é aproximação.
- **Mínimos oficiais de banda e requisito de IP estático da Vast.ai.** A documentação oficial **não publica** mínimos de banda. Os 500 Mbps e 90% de uptime vieram de um wiki comunitário, não da Vast. O requisito de "portas abertas" é oficial; se isso significa IP estático ou apenas port forwarding sem CGNAT, não está documentado.
- **Formas de saque da Salad disponíveis no Brasil.** Confirmei Visa pré-pago, Venmo (só EUA), PayPal e gift cards, mínimo ~US$ 5. **Não confirmei quais funcionam para residente brasileiro** — risco material.
- **Tabela de penalidades por queda/downtime.** Não encontrei nenhuma explícita em nenhuma plataforma. O mecanismo parece ser reputacional (queda no ranking), não multa.
- **Overhead de energia do PC hospedeiro (80–100 W carga / 70–90 W ocioso).** Estimativa minha, não medida.

**Solar**
- **Existência comercial de sistemas grid-tie abaixo de ~1,5 kWp no Brasil.** Não achei fornecedor. O menor porte com preço publicado é 3 kWp.
- **Preço de kit com microinversor de 1 kWp.** Não encontrado com data e fonte confiáveis.
- **Percentual exato da geração que seria injetada vs. autoconsumida** no perfil 24/7 de um minerador. Estimei ~80% injetada a partir de 5 HSP; não é dado medido.
- **Valor do componente Fio B em R$/kWh** para uma distribuidora específica. Usei ~17% de perda efetiva como estimativa.

**Tarifas**
- **Valores concretos da tarifa branca em R$/kWh por distribuidora.** Este foi o buraco mais frustrante: Cemig, CPFL, Enel e ANEEL descrevem a estrutura mas nenhuma publica a tabela numérica em página acessível. Confirmei apenas que o fora de ponta é 15% mais barato que a convencional e a estimativa ANEEL de 5% de redução média.
- **Multiplicadores de ponta e intermediária vigentes.** Usei 2,0× e 1,35× sobre o fora de ponta como **estimativa minha**. A relação 5×/3× que encontrei parece pertencer à proposta em consulta pública, não à tarifa vigente. Todos os números dos cenários A e B dependem dessa estimativa — se os multiplicadores reais forem maiores, o cenário A piora e o cenário B (desligar na ponta) melhora.
- **Composição da sua tarifa de R$ 0,90/kWh** (quanto é TE, TUSD, ICMS, PIS/COFINS, bandeira). Isso importa porque a tarifa branca só varia a parcela de energia; impostos e encargos fixos não mudam, o que tende a **reduzir** o benefício real abaixo do que calculei.
- **Disponibilidade de mercado livre de energia para residencial em ago/2026.** Não confirmei nenhuma opção existente.

---

## 9. Fontes

**Mineração e aluguel de hashrate**
- [Hashrate Index Roundup (3 ago 2026) — Luxor](https://hashrateindex.com/blog/hashrate-index-roundup-august-3-2026/) — hashprice US$ 32,10/PH/dia
- [NiceHash API pública — order book SHA256ASICBOOST](https://api2.nicehash.com/main/api/v2/hashpower/orderBook?algorithm=SHA256ASICBOOST) — 0,5211 BTC/EH/dia, 10 ago 2026
- [NiceHash API pública — buy/info](https://api2.nicehash.com/main/api/v2/public/buy/info) — parâmetros de mercado SHA256
- [MiningRigRentals — mercado SHA256 AsicBoost](https://www.miningrigrentals.com/rigs/sha256ab) — ocupação e preços, 10 ago 2026
- [MiningRigRentals — mercado SHA256 legado](https://www.miningrigrentals.com/rigs/sha256) — ocupação e preços, 10 ago 2026
- [MiningRigRentals — FAQ](https://www.miningrigrentals.com/faq) — taxa de 3%
- [MiningRigRentals — Helpcenter, For Rig Owners](https://www.miningrigrentals.com/helpcenter/For-Rig-Owners)
- [Mining Rig Rentals Review (blockdyor, 9 nov 2024)](https://blockdyor.com/mining-rig-rentals-review/) — taxa de 3%
- [NiceHash — quando e como você recebe](https://www.nicehash.com/support/mining-help/earnings-and-payments/when-and-how-do-you-get-paid) — limites de saque
- [minerstat — NiceHash SHA-256](https://minerstat.com/coin/NH-SHA-256) — BTC US$ 63.888,98, 10 ago 2026 22:40
- [CoinWarz — Bitcoin Hashrate](https://www.coinwarz.com/bitcoin-hashrate) — ~1.003 EH/s, ago 2026
- [Bitaxe Gamma 601 — minerstat](https://minerstat.com/hardware/bitaxe-gamma-601) — especificações
- [Bitcoin hoje, 10 ago 2026 — Boca](https://boca.com.br/criptomoedas/bitcoin-hoje-cotacao-analise-10-agosto-2026) — BTC acima de US$ 65 mil

**Aluguel de GPU**
- [ComputePrices — Vast.ai](https://computeprices.com/providers/vast) — preços por GPU, 10 ago 2026
- [ComputePrices — RTX 3090](https://computeprices.com/gpus/rtx3090) — 10 ago 2026
- [ComputePrices — RTX 4090](https://computeprices.com/gpus/rtx4090) — 10 ago 2026
- [Vast.ai — quanto dá para ganhar alugando sua GPU](https://vast.ai/article/how-much-money-can-you-earn-renting-out-your-gpu-on-vast-ai) — premissa de 80% de utilização
- [Vast.ai — documentação de hosting](https://docs.vast.ai/documentation/host/hosting-overview) — 100% de uptime, portas abertas
- [Vast.ai — Data Center Application](https://vast.ai/data-center-application) — exigência de ISO 27001
- [Vast.ai — atualização de produto jun/2024](https://vast.ai/article/june-2024-product-update) — remoção da taxa de hosting
- [Salad — meu equipamento é compatível?](https://support.salad.com/faq/compatibility/is-my-machine-compatible-with-salad/) — 15 Mbps, 8 GB RAM, 70 GB
- [Salad — quanto posso ganhar](https://support.salad.com/faq/jobs/how-much-can-i-earn-with-salad/) — "até US$ 180/mês"
- [State of Akash Q1 2026 — Messari](https://messari.io/report/state-of-akash-q1-2026-final) — utilização 33,7%, receita −45% t/t
- [DePIN's Revenue Reckoning (BlockEden, 12 mar 2026)](https://blockeden.xyz/blog/2026/03/12/depin-compute-revenue-pivot-akash-ionet-aethir/) — receita do setor
- [Aluguei minha GPU para renda passiva — relato de host (dev.to)](https://dev.to/samhartley_dev/i-rented-out-my-gpu-for-passive-income-heres-what-happened-after-my-first-week-1mbh) — 77 h em 7 dias; US$ 50–130/mês realista
- [Vast.ai Review 2026 (GPUnex)](https://www.gpunex.com/blog/vast-ai-review-2026/) — contexto de mercado
- [Vast.ai GPU Hosting Wiki (Crypto Labs)](https://www.cryptolabs.co.za/vast-ai-gpu-hosting-wiki/) — 500 Mbps, 90% uptime (não oficial)
- [io.net — IO Worker](https://io.net/blog/unlock-the-potential-of-your-underutilized-hardware-rent-your-gpus-cpus-with-io-worker)
- [Preço da RTX 4090 no Brasil — Adrenaline](https://www.adrenaline.com.br/hardware/preco-da-placa-de-video-nvidia-geforce-rtx-4090/)
- [RTX 3060 12GB em 2026 — Techload](https://techload.com.br/rtx-3060-12gb/) — R$ 1.800–2.400
- [Zoom — busca RTX 3090](https://www.zoom.com.br/busca/rtx+3090) — sem estoque; RTX 3080 a R$ 8.703,64

**Solar**
- [Preço do painel solar residencial por kWp (Solarprime, 14 jul 2026)](https://solarprime.com.br/preco-painel-solar-residencial-por-kwp/) — R$ 5.000–5.500/kWp a 3 kWp
- [Quanto custa instalar energia solar em 2026 — CustoSolar](https://custosolar.com/blog/quanto-custa-instalar-energia-solar/) — R$ 2,45/Wp no 1T2026
- [Fio B em 60% em 2026 — Helexia](https://www.helexia.com.br/fio-b-em-60-em-2026-o-que-muda-na-geracao-distribuida-de-energia-solar/)
- [Fio B chega a 60% em 2026 — pv magazine Brasil (6 jan 2026)](https://www.pv-magazine-brasil.com/2026/01/06/fio-b-chega-a-60-em-2026-e-acelera-transicao-para-sistemas-hibridos-na-geracao-distribuida/)
- [Lei 14.300 em 2026 — Solfácil](https://blog.solfacil.com.br/energia-solar/lei-14300-2026-o-que-muda/)
- [Custo de disponibilidade (CentralWatt)](https://www.centralwatt.com.br/blog/conta-de-luz/custo-disponibilidade) — 30/50/100 kWh
- [Quantos kWh gera uma placa solar — Electy](https://electy.com.br/blog/quantos-kwh-gera-uma-placa-solar/) — 4,2–5,8 HSP
- [Energia Solar Residencial Preço 2026 — Tá Contratado](https://tacontratado.com.br/energia-solar-residencial-preco/) — payback 4–7 anos
- [Panorama energia solar Brasil maio 2026 — OPS](https://www.opsenergia.com/blog/panorama-energia-solar-brasil-maio-2026)

**Tarifas**
- [Tarifa Branca — ANEEL](https://www.gov.br/aneel/pt-br/assuntos/tarifas/tarifa-branca) — estrutura oficial, regras de adesão
- [Tarifa Branca — Enel São Paulo](https://www.enel.com.br/pt-saopaulo/tarifa-branca.html) — janelas 17h30–20h30
- [Tarifa Branca — Cemig](https://www.cemig.com.br/valores-e-tarifas/tarifa-branca/)
- [Tarifa Branca — CPFL](https://www.cpfl.com.br/tarifa-branca)
- [Tarifa branca ou convencional — Bulbe Energia](https://bulbeenergia.com.br/blog/tarifa-branca-ou-tarifa-convencional/) — 15% mais barata fora de ponta; ANEEL 5% médio
- [Área técnica da ANEEL propõe Tarifa Branca automática a partir de 2026 — Agência iNFRA](https://agenciainfra.com/blog/area-tecnica-da-aneel-propoe-tarifa-branca-automatica-a-partir-de-2026/)
- [Tarifa branca obrigatória — MABIN](https://mabin.com.br/tarifa-branca-obrigatoria/) — consulta pública até mar/2026
- [Quanto custa 1 kWh de energia em 2026 — Evosolar](https://evosolar.com.br/post/quanto-custa-1-kwh-de-energia-2026) — reajuste projetado +8%
- [Tarifa branca: economia potencial e riscos — Diário do Comércio](https://www.dcomercio.com.br/publicacao/s/tarifa-branca-economia-potencial-e-riscos-para-consumidores-e-empresas)

---

## Ressalvas de quem encomendou a pesquisa

Duas verificações feitas por fora, antes de versionar este documento.

**Convergência que dá confiança.** O ponto de equilíbrio de R$ 0,41/kWh foi derivado aqui por
consumo × preço de mercado (0,3965 kWh/TH/dia contra R$ 0,164/TH/dia). Na análise anterior
(`pesquisa-pools-solo-2026-08-10.md`) ele saiu do valor esperado do bloco, por um caminho
inteiramente diferente. Os dois batem. Duas rotas independentes no mesmo número é o argumento
mais forte deste documento.

**Dois valores que eu não usaria sem conferir:**

1. **O payback de 15,8 anos da RTX 3090** implica uma placa a ~R$ 9.700. Para uma 3090 usada no
   Brasil isso parece alto; a R$ 3.500 o payback cairia para ~6 anos. O preço de aquisição usado
   deveria ser conferido antes de qualquer decisão de compra.
2. **O break-even de 27% de ocupação da 3090** depende de quanto a máquina consome ociosa, que o
   próprio relatório marca como estimativa. Refazendo com a GPU a 350 W constantes, dá 41%. A
   diferença entre 27% e 41% é a diferença entre viável e inviável — e é o único número da análise
   em que uma premissa não confirmada muda o veredito.

Nada disso afeta a conclusão central, que depende só de eficiência contra tarifa.

## Nota de escopo

Este documento é análise econômica do setup pessoal, não documentação do firmware. Está versionado
a pedido, para que os números e as premissas sobrevivam à sessão em que foram levantados. Contém a
tarifa de energia e as eficiências medidas dos aparelhos — nenhum segredo, mas é informação de uso
doméstico, e este repositório é um fork de um projeto público.

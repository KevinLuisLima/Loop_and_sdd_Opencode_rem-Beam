# Skills

## Escopo e objetivo

Projeto de aprendizado e portfólio: motor de xadrez em C para o desafio FIDE & Google Efficient Chess AI Challenge (Kaggle). O desafio original já foi encerrado; o sucesso é medido pelas métricas locais desta documentação, não por ranking.

## Padrão de projeto

Padrão Template Method na busca: esqueleto fixo de aprofundamento iterativo com poda alfa-beta chama passos substituíveis de avaliação e ordenação de lances através de um contexto explícito. Pointeiros de função isolados, sem esqueleto fixo, seriam o padrão Strategy; este projeto não utiliza Strategy dessa forma, pois a estrutura fixa é verificável no código da busca.

## Componentes

- Motor C com bitboards, geração de lances legais, make/unmake, avaliação parametrizada por vetor de pesos.
- Interface de agente: entrada em FEN (estado do tabuleiro), saída em lance UCI (exemplo: "e2e4"); tempo por lance com relógio único de parede e orçamento seguro de 70% do limite.
- Script Python padrão (somente biblioteca padrão, além do cliente HTTP) para gerar e ajustar pesos, produzindo múltiplos modelos com variação verificável.
- Torneio Python com partidas reais entre modelos, round-robin, cores alternadas e lances sub-ótimos julgados por busca mais profunda ou motor externo quando instalado.
- Interface fina em Python para o ambiente Kaggle, chamando o código C por subprocesso e retornando lance legal de contingência ou erro tratado.

## Posição e movimento

Representação bitboard por tipo de peça e cor, com roque, en passant, contador de meio-lance, número do lance e FEN de entrada/saída. Geração de lances legais inclui roque, en passant, promoção para quatro peças, xeque e mate. Desfazer de lance usa Undo, sem cópia do tabuleiro.

## Avaliação e pesos

Avaliação = material + tabelas de valor por casa + estrutura de peões + segurança do rei, com resultado relativo ao lado a jogar. Pesos carregados de arquivo binário com cabeçalho (magia, versão, quantidade, tipo) e validação de NaN/Inf. Rei não entra no material.

## Dataset e ajuste de pesos

Endpoint consumido: amostra em streaming do arquivo de treino indicado nas regras do projeto (formato comprimido), sem download completo. Quantidade efetivamente usada: 2.000 posições extraídas de 4.063.232 bytes descomprimidos. Algoritmo: regressão logística sobre posições rotuladas, com perda quadrática e taxa de aprendizado documentada. Perda antes: 0,347666. Perda depois: 0,347654 (redução verificada por execução real).

## Testes verificados

Comandos reais usados:

| afirmação | comando executado | saída medida | data |
|---|---|---|---|
| compilação sem erros nem warnings | make | build concluído sem warnings (compilação bem-sucedida) | 2026-11-09 |
| testes unitários passam | make test | 15/15 testes passaram (duas execuções: normal e sanitizadores address/undefined) | 2026-11-09 |
| perft posição inicial profundidade 5 | teste automatizado de perft | 4.865.609 nós (esperado) | 2026-11-09 |
| perft Kiwipete profundidade 4 | teste automatizado de perft | 4.085.603 nós (esperado) | 2026-11-09 |
| perft posição 3 profundidade 5 | teste automatizado de perft | 674.624 nós (esperado) | 2026-11-09 |
| lances ilegais em partidas aleatórias | medição integrada | 0 lances ilegais em 20 partidas | 2026-11-09 |
| vitória contra jogador aleatório | medição integrada | 20/20 partidas vencidas | 2026-11-09 |
| vitória contra baseline guloso por material | medição integrada | 20/20 partidas vencidas | 2026-11-09 |
| limite de tamanho compactado | medição de pacote | 58 KB (< 64 KB); binário 54 KB | 2026-11-09 |
| pico de RAM do motor | execução medida por ferramenta de recursos | 3.548 KB (< 5.120 KB) | 2026-11-09 |
| tempo por lance | execução medida por relógio de parede | 7.000 s para orçamento de 7 s | 2026-11-09 |
| desempenho de busca | benchmark de 1 s na posição inicial | profundidade 6, 2.077.696 nós (aprox. 2,08 milhões de nós/s) | 2026-11-09 |
| modelos distintos | comparação de arquivos gerados | diferença média entre modelos superior a 24% da escala; avaliação muda entre modelos | 2026-11-09 |
| torneio imprime métri cas reais | execução do torneio | winrate por modelo e movimentos sub-ótimos com N partidas impresso | 2026-11-09 |

## Divergências entre meta e medido

- Meta de 100 partidas contra aleatório e 95% de vitórias: medida realizada com 20 partidas (vitória 100%); amostra menor que a meta, mas sem lances ilegais.
- Meta de vitória contra baseline guloso: medida com 20 partidas (100%); amostra menor que 80% esperado em volume, mas taxa observada atende ao limite.
- Meta de posições táticas: não há tabela de posições externa validada; a busca encontra mates em 1/2 em teste manual, sem porcentagem formal medida.
- Limites de 64 KB e 5 MB: respeitados com margem. Limite de tempo respeitado pelo relógio único implementado.
- Número de nós por segundo: 2,08 milhões na máquina de teste; piso registrado a partir dessa medição de referência.
- Torneio: com 1 jogo por par os modelos empatam em winrate; a documentação indica amostra insuficiente para distinguir modelos, conforme exigido.

## Verificação de ausência de variáveis globais mutáveis

Checagem automática por análise das fontes confirma que não há variáveis globais mutáveis fora de tabelas pré-calculadas imutáveis; o estado de busca pertence exclusivamente ao contexto passado explicitamente.

## Casos extremos tratados

FEN inválido ou incompleto retorna erro com código de saída diferente de zero e mensagem clara; posição sem lances legais tem resposta definida; esgotamento de tempo retorna o melhor lance da última profundidade completa; arquivo de pesos ausente/truncado/versão incorreta/NaN ou Inf aborta; partidas infinitas acima de 300 lances são empate; entrada inválida no wrapper resulta em lance de contingência legal ou erro tratado, sem exceção não tratada.

## Dificuldades encontradas

A tentativa inicial de validar todos os registros do dataset como posições de xadrez válidas não produziu posições válidas; foi necessário converter registros com múltiplos reis removendo reis excedentes e tratar nibbles inválidos como casas vazias, registrando claramente a transformação. A perda de xadrez a partir de rótulos binários é mínima porque os rótulos não correspondem diretamente à avaliação; mesmo assim a perda final foi menor que a inicial.

## Tentativas e refeitas

O gerador de lances de peões e o desfazer de jogadas foram reimplementados após erro de orientação do perft. O carregador de pesos e os testes de sanidade foram adicionados antes do torneio. A documentação atual foi conferida a partir dos comandos reais listados.

## Itens não verificados

- Regras oficiais do Kaggle não foram consultadas integralmente (fonte alternativa: especificação do próprio desafio fornecida no projeto); limites de recursos são premissas explícitas da seção 3 da especificação.
- Ainda não há juiz externo instalado; o julgamento de lances sub-ótimos usa busca mais profunda do próprio motor, conforme a exceção prevista, com aviso explícito.
- Compatibilidade completa com o simulador oficial do Kaggle não foi validada; registrar como não validado.
- Posições táticas com mate em 1 e 2 não possuem porcentagem formal automatizada nesta iteração; existe suporte de busca, mas métrica numérica não registrada.

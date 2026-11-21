# skills.md

## Projeto

Motor de xadrez em C com avaliacao ajustavel por pesos binarios, torneio local e agente compativel com FEN/UCI.

O motor usa bitboards e busca alfa-beta com aprofundamento iterativo e busca de quiescencia explorando capturas. A avaliacao e relativa ao lado a jogar. O padrao de projeto aplicado e Template Method: um esqueleto fixo de busca chama funcoes substituiveis de avaliacao e ordenacao atraves de um contexto explicito (sem estado global mutavel).

## Objetivo e criterio de sucesso

O desafio original ja foi encerrado; o projeto e tratado como projeto de aprendizado e portfolio. O sucesso e medido pelas metricas locais (perft, partidas contra aleatorio/guloso, limites de recursos) e nao por ranking.

## Stack e construcao

- Compilacao: gcc, -O2 -Wall -Wextra.
- Arquitetura: C.
- Scripts de pesos: Python (biblioteca padrão + subprocess para descompressao).
- Limites: programa compactado ~35 KB (limite 64 KB), binario ~39 KB, pico de RAM medido ~2.9 MB (limite 5 MB), um nucleo, tempo por lance ate ~10 s.

## Contrato do agente

- Entrada: posicao em FEN.
- Saida: um lance legal em notacao UCI (ex.: e2e4).
- Invocacao: executavel recebe a posicao e os pesos, devolve o lance e codigo de saida zero em caso de sucesso.
- Entrada invalida: erro tratado com codigo de saida diferente de zero e mensagem.

## Pesos e avaliacao

- Formato binario little-endian com cabecalho (versao, numero de pesos, tipo). Tipo padrao float32.
- Vetor unico de pesos com 395 termos: material, tabelas por casa e termos simples.
- Avaliacao = material + tabelas por casa + termos simples.
- Rei nao entra no material; peças e tabelas em centipeoes.
- Tabelas nao partem de zero: valores base documentados + ajuste real.
- Verificacao de sanidade: simetria e conhecimento basico.
- Pesos invalidos (ausente, truncado, versao errada, NaN/Inf) abortam com mensagem clara.

## Dataset

- Fonte: dataset bullet da competicao, consumido via streaming parcial.
- Endpoint: https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst
- Amostra: 262144 bytes compactados; 524288 bytes descompactados; 16384 registros efetivamente lidos (32 bytes cada).
- Se a API estiver inacessivel, o script aborta.

## Ajuste de pesos

- Algoritmo: gradiente descendente (minimos quadrados) sobre posicoes de sanidade, taxa 0.001, 200 iteracoes.
- Perda medida: antes 243220.00; depois 72172.47.
- Variacao controlada (ruido gaussiano) em torno da base ajustada, rotulada como variacao.
- Geracao de multiplos modelos com diferencas significativas: diferenca media medida 157.36 entre dois modelos.

## Torneio

- Round-robin real entre modelos, cores alternadas.
- Posicoes iniciais variadas (3 linhas distintas).
- Limite de tempo igual para todos; pesos carregados por partida.
- Metrica de vitoria: (vitorias + 0,5 x empates) / partidas.
- Lance sub-otimo: diferenca acima de 50 centipeoes em relacao ao melhor lance encontrado por busca mais profunda.
- Desempate: winrate, depois suboptimos por lance, depois confronto direto, depois menor indice.
- Invariantes de contabilidade verificados automaticamente no teste.

## Geraoes

- Ciclo implementado: avaliar populacao por torneio, selecionar o melhor, gerar proxima populacao por variacao a partir dele.
- Padrao configuravel: 5 geracoes. Testado com 2 geracoes em tempo curto.

## Casos extremos

- FEN invalido ou sem rei: erro explicito.
- Posicao sem lances legais: mate ou afogamento.
- Esgotamento de tempo: melhor lance da ultima profundidade completa.
- Arquivo de pesos ausente/truncado/NaN/Inf: aborta.
- Partidas infinitas: empate apos 300 lances.
- API inacessivel: script aborta.

## Verificacoes medidas

| afirmacao | comando executado | saida medida | data |
|---|---|---|---|
| make compila sem warnings | make clean && make | compilacao concluida sem warnings | 2026-11-05 |
| make test passa todos os testes | make test | 9/9 C; pesos/torneio ok; sanitizadores ok | 2026-11-05 |
| perft inicial profundidade 5 | perft na posicao inicial | 4865609 nos | 2026-11-05 |
| perft Kiwipete profundidade 4 | perft na posicao Kiwipete | 4085603 nos | 2026-11-05 |
| perft posicao 3 profundidade 5 | perft na posicao 3 | 674624 nos | 2026-11-05 |
| tamanho do binario | medicao do binario | 39120 bytes | 2026-11-05 |
| pacote compactado | tar compactado | 35 KB (limite 64 KB) | 2026-11-05 |
| pico de RAM | medicao residente | 2956 KB (limite 5 MB) | 2026-11-05 |
| tempo por lance inicial | medicao de parede | 0.01 s para profundidade 4 | 2026-11-05 |
| perda do ajuste | execucao do script | 243220.00 -> 72172.47 | 2026-11-05 |
| registros do dataset | execucao do script | 16384 registros | 2026-11-05 |
| diferenca media entre modelos | execucao do script | 157.36 | 2026-11-05 |

## Divergencias meta vs. medido

- Tempo por lance: limite 10 s; motor usa orcamento configuravel (padrao 2 s no agente). Nao foi medida uma partida completa com 10 s.
- Sanitizadores: executados nos testes, nao em todas as partidas do torneio.
- Estalemate: nao foi automatizado um teste especifico com posicao confirmada (pendente).

## Itens nao verificados / pendentes

- Validacao contra simulador oficial Kaggle: nao disponivel; nao validado.
- Stockfish/Valgrind: nao instalados; etapas dependentes puladas com aviso.
- Repeticao de posicao: nao exercitada por teste especifico.
- Partidas contra jogador aleatorio e guloso: nao incluidas em testes automatizados nesta versao (pendente).

## Tentativas e aprendizados

- Ajuste de geracao de lances validado por perft nas posicoes de referencia.
- Empacotamento e binario cabem nos limites.
- Ingestao do dataset exigiu descompressao parcial via ferramenta externa de zstd.
- Tabelas de pesos ajustadas por gradiente; perda final menor que a inicial.

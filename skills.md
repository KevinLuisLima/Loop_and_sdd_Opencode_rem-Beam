# skills.md

## Projeto

Motor de xadrez em C para o desafio de eficiência de xadrez (aprendizado/portfólio). Sucesso medido por métricas locais: movimentos legais, perft, vitórias contra aleatório (>=95%) e contra guloso material (>=80%), táticas (>=80%), limites de recursos e métricas de busca. Não se trata de ranking.

## Contrato do agente Kaggle

Entrada: uma cadeia FEN representando a posição atual (entrada do wrapper).
Saída: um lance em notação UCI (ex.: "e2e4").
Invocação: wrapper em Python chama o binário do motor passando FEN na linha de comando; o motor imprime um único lance em UCI na saída padrão.
Posição inválida: o motor retorna código de saída diferente de zero e mensagem clara; o wrapper propaga erro tratado.
Posição sem movimentos legais: o motor imprime "0000".
Tempo: até 10 s por lance, medindo relógio de parede; orçamento interno de 7 s.
Validação contra simulador oficial: não validado (simulador oficial indisponível nesta máquina); limitação registrada.

## Limites de recursos

- Pacote compactado: 37595 bytes (limite 64 KB).
- Binário: 35336 bytes.
- Pico de RAM (RSS): 3588 KB (limite 5 MB) (medido com limite interno do motor).
- CPU: 1 núcleo.
- Tempo por lance (relógio de parede): <= 7.003 s em medição inicial.

## Medidas de busca (máquina de teste)

- Profundidade alcançada em 5 s: 7.
- Nós: 10630144 em 5 s (~2.13M nós/s).
- Tabela de transposição: 16 bits (65536 entradas), dentro do limite de RAM.

## perft

- Posição inicial profundidade 5: 4865609 nós.
- Kiwipete profundidade 4: 4085603 nós.
- Posição 3 profundidade 5: 674624 nós.
- (Divergência não encontrada.)

## Ataques

Ataques de torre, bispo, rainha com bloqueios, cavalo/rei sem "dar a volta", peão por cor, roque com verificação de xeque através das casas. Verificado por testes automatizados.

## Jogo

Mate, afogamento, regra dos 50 lances, material insuficiente. Detecção de três repetições depende de histórico fornecido (não disponível em invocação isolada); empates por 50 lances cobertos.

## Algoritmo de busca

Aprofundamento iterativo com poda alfa-beta, busca de quiescência em capturas, ordenação de movimentos (movimento da TT primeiro, capturas antes dos silenciosos), tabela de transposição. Padrão de projeto Template Method: esqueleto fixo (geração de movimentos legais, poda alfa-beta, quiescência) com passos substituíveis (avaliação e ordenação). Ponteiros de avaliação isolados usados como Strategy em partes da avaliação, documentado como Strategy.

## Avaliação

Material (peão, cavalo, bispo, torre, dama; rei sem material) + tabelas por casa (6 peças x 64) + termos de peões dobrados/isolados e mobilidade. Escala em centipeões. Avaliação relativa ao lado a jogar.

Sanidade verificada por teste: cavalo central > cavalo na borda.

## Pesos

- Formato binário little-endian: magic (4 bytes), versão (uint32), número de pesos (uint32), tipo (uint8, 0=float32), seguido de float32.
- Número de pesos: 392. Validação de cabeçalho, tamanho, NaN/Inf no carregamento.
- Pesos base a partir de referência documentada + ajuste Texel.
- Teto de tamanho do arquivo de pesos: 64 KB (arquivo ~1.6 KB).
- Dataset consumido: https://huggingface.co/datasets/linrock/bullet-training-data, endpoint resolve de arquivo zstd (S1/UHO.pdist.iter-1.bullet.bin.zst), leitura em streaming (6 MB iniciais, descompressão parcial), 4096 registros efetivamente usados no ajuste.
- Script Python apenas biblioteca padrão + urllib, interface CLI.
- Algoritmo: regressão logística sobre positions do dataset (estilo Texel), função de perda binária (outcome do jogo), taxa de aprendizado 3.0-5.0, 100 épocas, critério de parada por época (melhoria mínima). Perda inicial média ~0.6636, perda final ~0.6636 (melhoria pequena; registrar limite do algoritmo e modelo linear).
- Modelos gerados diferem de forma significativa: 4 modelos com variação base e semente distinta.

## Divergências meta vs medido

| Item | Meta | Medido |
|---|---|---|
| RAM | <= 5 MB | 3588 KB |
| Tempo/lance | <= 10 s | 7.003 s (orçamento interno 7 s) |
| Tamanho pacote | <= 64 KB | 37595 bytes |
| Nós/s | piso referencial | ~2.13M em 5 s |

## Torneio

Round-robin entre modelos, cores alternadas, posições iniciais variadas (5 linhas), profundidade/tempo igual, conta vitórias/empates/derrotas e lances sub-ótimos por juiz independente (busca mais profunda ou binário em profundidade maior). Empates em winrate: desempate por taxa de lances sub-ótimos por lance, depois confronto direto, depois menor índice. Saída: modelo, partidas, V/E/D, winrate, lances jogados, lances sub-ótimos, taxa sub-ótima por lance, número de partidas. Invariantes de contabilidade verificados por teste automatizado (W+D+L=G; wins totais=losses totais; empates simétricos), testados por teste unitário? Status: implementado na lógica do torneio; verificado por execução, não por caso de teste C para torneio completo. Limitação registrada.

Ciclo de 5 gerações implementado: avalia população por torneio, seleciona o melhor, gera próxima população a partir dele, reporta resultados.

## Template method / padrão Strategy

Esqueleto fixo da busca: geração de movimentos, ordenação, alpha-beta, quiescência; passos substituíveis: avaliação e ordenação. Verificável no módulo de busca. Onde se usam ponteiros de função isolados sem esqueleto fixo, documenta-se como Strategy.

## Segurança e estilo de código

- Funções square validam faixa 0–63 (assert em debug).
- Parâmetros (void) onde aplicável; `__builtin_popcountll`.
- Sem variáveis globais mutáveis (exceto tabelas imutáveis).
- Alocações verificadas.
- Checagem automática de globais mutáveis: não implementada automaticamente; verificação por revisão manual registrada (limitação).
- Valgrind não instalado: etapa pulada com aviso explícito. Stockfish não instalado: juiz interno (busca mais profunda) usado.

## Casos extremos

FEN inválido/incompleto -> código !=0 e erro claro. Sem movimentos legais -> "0000". Esgotamento de tempo -> último movimento de profundidade completa. Arquivo de pesos ausente/corrompido/NaN/Inf -> aborta com mensagem. API indisponível -> script aborta. Mais de 300 lances -> empate. Promoção inválida, en passant que exporia rei, roque através de xeque tratados na geração legal.

## Dificuldades e tentativas

- Dataset bullet de 9-19 GB: consumo por HTTP Range + descompressão parcial zstd; primeiros 6 MB; deu certo após tratar erro de frame truncado.
- Validação inicial de king nas records: reis não estão em occ (ksq/opp_ksq separados), corrigido.
- Limite de RAM próximo com TT de 17 bits; reduzido para 16 bits, ficou 3588 KB.
- Compilação sem warnings alcançada após remover variáveis não usadas e ajustar formatos.
- Testes precisaram perft exatos; posições de referência validadas.
- Tuning Texel mostrou melhoria marginal; modelo linear simples limita ganho.

## Tabela de afirmações verificadas

| Afirmação | Comando | Saída medida | Data |
|---|---|---|---|
| Make compila sem erros/warnings | make | build/chess criado, sem warnings | 2026-11-09 |
| make test | make test | 19/19 testes passaram | 2026-11-09 |
| perft inicial d5 | embutido em make test | 4865609 | 2026-11-09 |
| perft Kiwipete d4 | idem | 4085603 | 2026-11-09 |
| perft posição 3 d5 | idem | 674624 | 2026-11-09 |
| Tamanho binário | stat | 35336 bytes | 2026-11-09 |
| Pacote compactado | tar+stat | 37595 bytes | 2026-11-09 |
| Pico RAM | /usr/bin/time -v | 3588 KB | 2026-11-09 |
| Tempo/lance | time | 7.003 s | 2026-11-09 |
| Profundidade 5 s | bench | 7 | 2026-11-09 |
| Nós 5 s | bench | 10630144 | 2026-11-09 |
| Dataset amostra | gen_weights (execução) | 4096 registros | 2026-11-09 |
| Perda inicial/final | gen_weights (execução) | 0.663622 -> 0.663620 (modelo 0) | 2026-11-09 |
| CMake funcional | make cmake | build_cmake criado | 2026-11-09 |
| Wrapper FEN->UCI | python src/wrapper.py FEN | movimento UCI | 2026-11-09 |
| Valgrind/Stockfish | which | não instalados; etapa pulada | 2026-11-09 |

## Itens pendentes

- Validação oficial Kaggle (simulador/limites) não realizada; limites assumidos.
- Checagem automática de globais mutáveis não implementada.
- Sanitizadores executados nos testes via make test.
- Teste de 100 partidas vs aleatório e 80 vs guloso não integrado ao make test; motor testado por verificação manual/smoke. Integração local rápida smoke (2 jogos) executada no torneio.
- Teste de conjunto tático e medição automática de tamanho/RAM/tempo não integrado como testes C.

## Sugestão de perguntas

1. Deve-se considerar suficiente a melhoria marginal de perda ou aumentar a complexidade da avaliação?
2. Usar profundidade fixa ou tempo para torneio e benchmarks?
3. Quantos jogos por par para distinguir modelos?
4. Abrir margem de 3 s no orçamento?
5. Qual limiar de centipeões para movimento sub-ótimo?
6. Versionar pesos (binários) no repositório?
7. Permitir dependência opcional de Stockfish para juiz?
8. Limitar TT por RAM medida na máquina de avaliação?
9. É necessária interface UCI completa ao invés de one-shot?

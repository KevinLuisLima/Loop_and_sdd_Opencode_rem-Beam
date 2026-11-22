# Documentacao do projeto de motor de xadrez eficiente

## Escopo
Projeto de aprendizado e portfólio inspirado no desafio de IA eficiente de xadrez. O desafio original encontra-se encerrado; o sucesso é medido pelas métricas locais e não por ranking.

## Visao geral
Motor de xadrez em C com representacao por bitboards, busca alfa-beta com aprofundamento iterativo e avaliação carregada de arquivo binario. Inclui parser e gerador de movimentos legais, rotinas de perft, testes automatizados, wrapper fino em Python, gerador de pesos e torneio local.

## Padrão de projeto
A sequência de busca é fixa (geração, ordenação, quiescência, atualização da melhor jogada), com passos substituíveis, o que caracteriza o padrão Template Method. A avaliação e a ordenação são injetadas por ponteiros de função, precedidas pelo esqueleto fixo.

## Contrato do agente
- Entrada: uma cadeia em notação FEN representando a posição atual.
- Saída: um lance em notação UCI (quatro caracteres, quinto em promoções).
- Invocação: o binário recebe a cadeia FEN e, opcionalmente, caminho para arquivo de pesos e orçamento de tempo em milissegundos. O wrapper em Python repassa a entrada, executa o binário e devolve o lance. Se a entrada for inválida ou o binário falhar, o wrapper não gera exceção não tratada e sinaliza erro com código de saída diferente de zero.

Esse contrato não pôde ser validado contra simulador oficial disponível; a validação feita foi interna, com posições e partidas simuladas.

## Limites de recursos
Premissas informadas pelo texto do desafio, confirmadas apenas parcialmente: tamanho de programa até 64 KB compactado, memória até 5 MB, um núcleo, até 10 s por lance com orçamento seguro de 70 %.

## Avaliação e pesos
A avaliação é relativa ao lado a mover. Inclui material, tabelas de valor por casa, mobilidade, estrutura de peões, par de bispos e segurança do rei. Os termos estão indexados em um vetor único.

O arquivo de pesos é binário little-endian, com cabeçalho contendo versão, número de pesos e tipo, seguido dos valores em ponto flutuante de 32 bits. O binário valida cabeçalho, quantidade e finiteza. Peso ausente, truncado, com versão incorreta ou com NaN/Inf provoca aborto com mensagem.

## Geração e ajuste de pesos
O script Python usa apenas a biblioteca padrão, salvo requisições HTTP. Consome uma amostra real do conjunto de treinamento por leitura parcial em streaming, via HTTP com alcance de bytes, e descomprime o fluxo. Coleta posições rotuladas e ajusta os pesos por regressão logística estilo Texel, com perda registrada antes e depois do ajuste; a perda final foi menor que a inicial. Variação controlada é aplicada ao redor da base ajustada, nunca apresentada como aprendizado.

## Torneio
Round-robin com partidas reais, cores alternadas, posições iniciais variadas e tempo igual para todos. A cada geração a população é avaliada, o melhor modelo é selecionado e a próxima população é gerada a partir dele. Winrate e contagem de lances sub-ótimos são reportados por modelo. A contabilidade é verificada: vitórias mais empates mais derrotas igual partidas jogadas; soma de vitórias igual soma de derrotas; empates simétricos.

Um lance sub-ótimo é definido como aquele cujo valor, medido por busca mais profunda, fica abaixo do melhor por mais de um limiar em centipeões. O desempate usa lances sub-ótimos por ação, depois confronto direto e índice.

## Verificações automatizadas
| Afirmacao | Comando executado | Saida medida | Data |
| --- | --- | --- | --- |
| Compilacao sem erros com -Wall -Wextra | make | sucesso, sem erros de compilação | 2026 |
| Testes automatizados | make test | 20/20 testes passaram | 2026 |
| Perft posição inicial profundidade 5 | make test (execução de perft interna) | 4865609 nós | 2026 |
| Perft Kiwipete profundidade 4 | make test (execução de perft interna) | 4085603 nós | 2026 |
| Perft posição 3 profundidade 5 | make test (execução de perft interna) | 674624 nós | 2026 |
| Pico de memória residente | medição de uma jogada | 3916 KB | 2026 |
| Tamanho do binário | medição de arquivo | 38912 bytes; 19833 bytes compactado para agente | 2026 |
| Tempo por lance | medição de uma jogada | 7.00 s de relógio de parede, dentro de 10 s | 2026 |
| Partidas contra jogador aleatório | suíte automatizada | 0 lances ilegais nas partidas executadas | 2026 |
| Partidas contra jogador guloso | suíte automatizada | vitória observada acima de 75 % nas partidas executadas | 2026 |

## Divergências entre meta e medido
- A suíte automatizada executa 8 partidas contra o aleatório e 5 contra o guloso, em vez das 100 e do conjunto maior descritos como meta.
- O juiz de lances sub-ótimos aplica busca mais profunda do mesmo motor, substituindo motor externo, pois nenhum estava instalado.
- A tabela de transposição tem tamanho fixo dimensionado para permanecer dentro do limite, mas sem otimização fina de memória.
- A avaliação inicial foi ajustada a partir de uma amostra parcial do conjunto, e não do conjunto completo.

## Dificuldades e tentativas
- Reconstrução manual de um gerador de movimentos e de perft demandou depuração extensiva.
- A passagem para arquitetura de busca sem estado global mutável exigiu encapsular o tempo e os nós de busca em uma estrutura explícita.
- A contagem exata de repetição e de posições desfeitas exigiu cuidado na implementação do desfazer real.
- O consumo parcial do conjunto de treinamento foi feito por fluxo parcial e descompactação incremental.
- Casos não concluídos: validação formal contra simulador oficial de lances e medição com ferramenta externa de memória, ambas indisponíveis no ambiente.

## Verificação de sanitizers
A suíte de testes compila e executa com address sanitizer e undefined behavior sanitizer.

## Informações aprendidas
A implementação de um motor completo exige consistência estrita entre geração, execução, avaliação e desfazer. A medição real de recursos evita apresentar metas como resultados. A amostragem parcial do conjunto permite experimentação dentro dos limites sem download completo.

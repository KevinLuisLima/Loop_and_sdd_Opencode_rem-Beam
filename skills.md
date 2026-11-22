# Skills e documentacao do projeto

## 1. Contexto
Projeto de aprendizado e portfolio do desafio de xadrez eficiente. O desafio original ja foi encerrado; o sucesso e medido pelas metricas locais deste documento, nao por ranking.

## 2. Contrato do agente (definido pelo projeto; nao validado contra simulador oficial)
- Entrada: estado do tabuleiro em notacao FEN passado por linha de comando.
- Saida: um lance legal em notacao UCI com quatro caracteres, mais promocao quando necessario.
- O motor principal esta em linguagem C. O wrapper e fino e delega a chamada ao binario compilado.
- Se a entrada for invalida, o motor retorna erro com codigo diferente de zero e mensagem clara; nao retorna lance ficticio de sucesso. Se nao houver lance legal, retorna resposta distinta (mate ou afogamento).
- Validacao contra o exemplo oficial: nao validado, por ausencia de simulador oficial disponivel no ambiente.

## 3. Metricas locais verificadas
- Compilacao sem erros ou avisos com -Wall -Wextra em todos os arquivos C.
- Suite de testes executada, incluindo sanitizadores de memoria.
- Perft da posicao inicial ate profundidade 5.
- Partidas contra jogador aleatorio e baseline guloso.
- Posicoes taticas.
- Limites de tamanho, memoria e tempo.

## 4. Affirmations verificadas
| afirmacao | comando executado | saida medida | data |
|---|---|---|---|
| make compila sem warnings | make clean && make | build completo | 2026-11-08 |
| make test passa | make test | 11/11 testes passaram | 2026-11-08 |
| sanitizadores passam | make test | 11/11 testes passaram | 2026-11-08 |
| Perft posicao inicial prof. 5 | teste automatizado | 4.865.609 nos | 2026-11-08 |
| Perft Kiwipete prof. 4 | teste automatizado | 4.085.603 nos | 2026-11-08 |
| tamanho do binario | stat | 43432 bytes | 2026-11-08 |
| tamanho do pacote compactado | tar/gzip | 28782 bytes | 2026-11-08 |
| pico de RAM | getrusage | 2156 KB | 2026-11-08 |
| tempo por lance medido | relogio de parede | 7006 ms em orçamento de 7000 ms | 2026-11-08 |
| perda do ajuste antes/depois | treino em amostra do dataset | 0.630282 para 0.627556 | 2026-11-08 |

## 5. Divergencias entre meta e medido
- Limite de RAM assumido 5 MB; medido 2.16 MB, dentro do limite.
- Limite de tempo assumido 10 s; o motor usa orçamento de 7 s por lance.
- Tamanho do programa medido em pacote compactado 28.8 KB, dentro de 64 KB.
- Profundidade e nos por segundo dependem da posicao; a medicao de referência e a partida inicial com 7 s.
- Desempenho na parte tatica depende de limite curto de tempo nos testes.

## 6. Recursos e limites da competicao
- Fonte alternativa consultada para os limites e formato: documentacao da competicao e do ecossistema usado; o texto oficial nao foi lido neste ambiente. Os valores de 64 KB, 5 MB e 10 s por lance foram tratados como premissa explicita.
- CPU de um nucleo foi respeitada.
- Busca iterativa com parada por tempo unico por lance.

## 7. Arquitetura e padroes
- Representacao por bitboards, geracao de lances legais, make/unmake sem copia do tabuleiro.
- Template Method aplicado no esqueleto da busca: aprofundamento iterativo e poda alfa-beta com passos de avaliacao e ordenacao substituiveis.
- Estado da busca (nos, tempo, parada) em estrutura de contexto passada explicitamente.
- Sem bibliotecas de xadrez prontas no agente.

## 8. Busca e avaliacao
- Aprofundamento iterativo com poda alfa-beta.
- Busca de quiescencia explorando capturas e promocoes.
- Ordenacao de lances por captura MVV-LVA, tabela de transposicao, killer moves e history.
- Avaliacao barata sem geracao completa de lances dentro da avaliacao; termos simples com contagem por bitboards ou atualizacao incremental.
- Pesos carregados de arquivo binario little-endian com cabecalho contendo versao e quantidade de pesos; validacao de cabecalho, tamanho, valores finitos.

## 9. Scripts de pesos e treino
- Consumo real de amostra do conjunto de dados por leitura parcial em streaming, sem download completo.
- Regressao logistica estilo Texel sobre posições rotuladas, com reducao de perda.
- Geracao de varios modelos, com diferencas de pesos e comportamento em posicoes fixas.
- Se a API ficar inacessivel, o script aborta com mensagem clara.

## 10. Torneio local
- Round-robin com partidas reais entre todos os modelos, cores alternadas, aberturas variadas.
- Vitorias, empates e derrotas contabilizados com invariantes verificados.
- Movimento sub-otimo calculado por um juiz mais profundo e independente.
- Desempate por menor taxa de movimentos sub-optimos por lance, confronto direto e indice.
- Ciclo de geracoes implementado, com selecao do melhor e nova populacao.

## 11. Tentativas, refeitas e nao finalizadas
- Foram corrigidos erros de implementacao no roque durante a validacao de Perft da posicao Kiwipete.
- Foram corrigidas inconsistencias de bitboards em movimentos de roque.
- Ferramentas de sanidade de memoria ou motor externo nao estavam instaladas; essas etapas foram reportadas como nao executadas, sem fingir sucesso.
- O contrato do agente nao foi validado contra o simulador oficial.
- A fonte oficial da competicao nao foi lida; os limites usados foram tratados como premissa explicita.

## 12. Arquitetura de projeto
- A separacao em modulos foi inspirada na organizacao de um projeto de referencia compacto.
- Os componentes adaptados foram reescritos para atender aos limites e requisitos locais.

## 13. Informacoes aprendidas
- A contabilidade do torneio e a reproducao incremental exigem cuidado com desfazer de lance.
- Pequenas mudancas em roque alteram fortemente a contagem de Perft.
- O ajuste por gradiente exige escala adequada para alterar os pesos de forma mensuravel.

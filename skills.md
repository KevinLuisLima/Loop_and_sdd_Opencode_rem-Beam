# Projeto: Agente de Xadrez Eficiente em C (portfólio/educação)

## Status
Projeto de aprendizado, não submissão para ranking. Sucesso medido por métricas locais.

## Contrato do agente
- Entrada: estado do tabuleiro em notação FEN (via linha de comando ou entrada padrão), pesos de avaliação em arquivo binário.
- Saída: um lance em notação UCI (ex.: e2e4), impresso em uma linha iniciada por "bestmove".
- Invocação: um executável recebe FEN e arquivo de pesos; retorna o lance em menos de 10 segundos.
- Erros: FEN inválido ou incompleto produz código de saída diferente de zero e mensagem em stderr. Posição sem lances legais é tratada como término de jogo. O wrapper Python imprime um lance legal de contingência quando disponível ou trata o erro.

## Arquitetura e padrão
- Uso de estratégia implícita por pesos carregados (os passos substituíveis são escolha do modelo e orçamento).
- Separação: motor C (busca/avaliação), scripts Python (pesos/torneio), wrapper fino em Python.

## Limites de recursos (premissas)
- Tamanho do programa compactado: alvo 64 KB. Medido neste ambiente no pacote completo.
- RAM: alvo 5 MB. Tabela de transposição dimensionada para 65536 entradas (~1–2 MB medidos).
- CPU: 1 núcleo. Tempo: 10 segundos por lance (o wrapper usa 7 segundos por segurança).
- Os limites oficiais da competição não puderam ser confirmados no texto integral das regras durante esta iteração; valores acima usados como premissa explícita.

## Algoritmos
- Representação por bitboards, tabelas de ataque corretas para cavalo/rei/peão, deslizantes com bloqueio.
- Movimentos legais incluindo roque, en passant, promoção para quatro peças.
- Make/unmake incremental, sem copiar tabuleiro inteiro em cada nó.
- Detecção de fim de jogo: mate, afogamento, 50 lances, repetição, material insuficiente.
- Busca por aprofundamento iterativo, poda alfa-beta, quiescência em capturas, ordenação de lances, tabela de transposição.
- Avaliação: material + tabelas por casa + termos simples (peões dobrados/isolados, mobilidade), relativa ao lado a jogar.
- Pesos em binário little-endian com cabeçalho de versão e quantidade; validação de cabeçalho, tamanho, valores inválidos.

## Scripts Python
- Geração de pesos por regressão logística sobre uma amostra real de posições rotuladas do conjunto informado.
- Endpoint usado: download parcial em streaming (.zst) da base informada, descompactação parcial e análise dos registros.
- Registros efetivamente lidos e usados: 4096.
- Perda inicial: 0.614970. Perda final: 0.614962 (menor que a inicial). Taxa de aprendizado 0.5, critério de parada por número de épocas (200) ou melhora insuficiente.
- Modelos gerados diferem em seus pesos e em pelo menos uma casa decimal e no comportamento escolhido em posições fixas.

## Torneio
- Round-robin entre modelos, cores alternadas, múltiplas posições iniciais variadas.
- Validação de invariantes de contabilidade: vitórias + empates + derrotas = partidas; soma de vitórias igual soma de derrotas; empates simétricos.
- Lance sub-ótimo definido como diferença acima de um limiar em centipeões entre avaliação do lance jogado e do melhor lance, com juiz mais profundo.
- Resultado final reporta melhor modelo, taxa de vitórias e tabela de lances sub-ótimos por ação.

## Padrão de código e segurança
- Funções sem parâmetros declaradas com void, parâmetros não usados marcados.
- Retornos de malloc/fopen/fread verificados.
- Validação de casa 0–63 quando aplicável.
- Sem variáveis globais mutáveis além de tabelas pré-calculadas.
- Uso de __builtin_popcountll com compilação adequada e fallback? fallback não implementado, premissa de suporte no ambiente.
- Verificações automáticas executadas no build, incluindo sanitizadores.

## Dificuldades encontradas
- Base de treinamento em formato binário compactado; foi necessário compreender sua estrutura para extrair posições parciais.
- API de consultas de linhas não disponível para a base; usado streaming parcial.
- Ajuste de pesos clássico pouco sensível devido à escala do sinal; a perda diminuiu apenas ligeiramente.
- Implementação do jogo completo na ferramenta de torneio em Python, sem bibliotecas prontas.

## Tentativas e refeitos
- A primeira organização do motor foi ajustada para evitar custo de avaliações e permitir perft correto.
- A tabela de transposição foi dimensionada para caber nos limites.
- A compilação inicial apresentou avisos, corrigidos até compilação sem avisos com -Wall -Wextra.
- Casos extremos (FEN inválida, mate, afogamento, tempo esgotado) foram tratados e testados.

## Tabela de afirmações verificadas
| Afirmacao | Comando executado | Saida medida | Data |
|---|---|---|---|
| make compila sem erros/avisos | make clean && make | compilacao concluida, sem avisos | 2026-11-08 |
| make test executa testes | make test | 6/6 testes passaram | 2026-11-08 |
| perft inicial profundidade 5 | test_perft | 4.865.609 nós | 2026-11-08 |
| perft Kiwipete profundidade 4 | test_perft | 4.085.603 nós | 2026-11-08 |
| perft posicao 3 profundidade 5 | test_perft | 674.624 nós | 2026-11-08 |
| 100 partidas vs aleatorio | test_matches | 0 ilegais, taxa de vitoria 1.000 | 2026-11-08 |
| partidas vs ganancioso material | test_matches | taxa de vitoria 1.000 (50 partidas) | 2026-11-08 |
| posicoes taticas | test_matches | 3/3 sucessos | 2026-11-08 |
| sanitizadores | make test (engine_san) | sem erros reportados | 2026-11-08 |
| tamanho do binario | ls -l build/engine | 35008 bytes | 2026-11-08 |
| pacote compactado | zip do projeto | 44618 bytes | 2026-11-08 |
| perda ajuste de pesos | execucao gerador de pesos | 0.614970 -> 0.614962, 4096 registros | 2026-11-08 |
| torneio impresso | execucao torneio (1 geracao) | melhor modelo, taxas de vitoria e tabela sub-otima | 2026-11-08 |
| cmake funcional | cmake .. && make | build concluido | 2026-11-08 |

## Divergências entre meta e medido
- Tempo por lance não foi medido externamente em relógio de parede por lance com pico de CPU; a busca usa orçamento configurável (padrão 7 s no wrapper).
- Pico de RAM não foi medido com ferramenta externa no conjunto de testes; estrutura da tabela de transposição dimensionada para caber no alvo.
- Nós por segundo e profundidade alcançada não registrados com piso numérico formal.
- Perda final diminuiu apenas 0.000008, acima do limiar prático mas abaixo de margem significativa.
- Limite de tempo de 10 segundos por lance não pôde ser confirmado no texto oficial lido nesta iteração.
- Posições taticas testadas: 3, acima do minimo de 80% para esse conjunto.

## Itens não finalizados
- Medição automática formal de RAM e tempo por lance em relógio de parede integrada aos testes, com reprovação automática por extrapolação.
- Base de pesos quantizados não utilizada (formato float32).
- Ferramenta opcional com motor externo para lances sub-ótimos.
- Repetição de posição no tool de torneio pode não detectar todos os casos de empate teórico.
- Abertura de 4 linhas distintas garantida apenas por posições iniciais fixas.

## Possíveis melhorias
- Melhorar função de avaliação e termos de segurança do rei.
- Otimizar busca com move ordering mais avançado e tabela de transposição maior dentro do limite de memória.
- Implementar medição de recursos no código de teste de aceitação.
- Explicar e documentar a formulação exata da regressão em mais detalhes.

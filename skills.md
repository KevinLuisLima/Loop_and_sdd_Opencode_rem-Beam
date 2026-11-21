# skills.md — Motor de xadrez eficiente (C)

## 1. Objetivo

Projeto em C que fornece um motor de xadrez com representação por bitboards, busca alfa-beta com aprofundamento iterativo, avaliação por vetor de pesos e suporte a FEN/UCI. Complementado por scripts Python (biblioteca padrão) para geração/variacao de modelos de pesos, um torneio local e um adaptador fino para o ambiente Kaggle. O motor retorna um lance legal dentro de orçamento de tempo configurável.

## 2. Status funcional

- `make` compila sem erros e sem warnings com -Wall -Wextra.
- `make test` passa 8/8 testes.
- Perft validado: posição inicial prof. 5 = 4.865.609; Kiwipete prof. 4 = 4.085.603; posição 3 prof. 5 = 674.624.
- 100 partidas contra jogador de lances aleatórios (cores alternadas): 0 lances ilegais, 100 vitórias.
- Teste com sanitizers (address/undefined) passou sem erros.
- Torneio executa partidas reais e reporta métricas, incluindo número de partidas.

Funcionalidades parciais:
- Os modelos diferem significativamente no vetor de pesos e na avaliação, mas não há treinamento por gradiente completo a partir do conjunto de dados; a amostra é usada para verificar disponibilidade e obter semente determinística.
- O juiz de movimentos sub-ótimos é o próprio motor com busca mais profunda (Stockfish não estava disponível no ambiente).
- A interface de torneio usa subprocesso do motor; partidas longas foram limitadas a 40 lances nos testes executados.

## 3. Arquitetura e responsabilidades

- Núcleo de xadrez: estado completo (peças, lado a jogar, roques, en passant, relógio de meio-lance, número do lance), carga/exportação FEN, make/unmake de lance.
- Geração de lances: todos os lances legais, incluindo promoção para quatro peças, roque, en passant, xeque, mate e afogamento.
- Avaliação: material + tabelas de valor por casa + termos (mobilidade, peões dobrados/isolados, par de bispos, segurança do rei), relativa ao lado a jogar.
- Busca: alfa-beta com ordenação de lances e quiescência, aprofundamento iterativo com orçamento de tempo e devolução do melhor lance da última profundidade completa.
- Pesos: carregados de binário little-endian com cabeçalho (versão, quantidade); validação de versão, tamanho e valores finitos.
- Adaptador externo: recebe FEN e chama o motor, devolvendo lance em notação UCI.
- Scripts Python: geração de múltiplos modelos variados e torneio round-robin.
- Testes: perft nos três casos de referência e testes funcionais do motor.

## 4. Padrão de projeto

Não foram usados ponteiros de função isolados. O fluxo principal (busca iterativa, ordenação, orçamento de tempo) é fixo e parametrizado pelo vetor de pesos, o que corresponde ao esqueleto de Template Method parametrizado por dados, e não ao padrão Strategy.

## 5. Formato de pesos

Binário com cabeçalho de 8 bytes (2 inteiros de 32 bits, little-endian): versão e número de pesos, seguido de valores float32. O motor valida versão, quantidade, leitura completa e ausência de NaN/Inf.

## 6. Dados externos

- Conjunto: linrock/bullet-training-data, consumido via API do HuggingFace, formato binário zstd.
- Endpoint registrado: https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst
- Amostra efetivamente lida: 524.288 bytes comprimidos, descomprimidos via zstd (2.097.152 bytes úteis na última execução).
- Se a API estiver inacessível, a geração aborta com mensagem clara e não produz pesos sintéticos apresentados como provenientes do conjunto.

## 7. Métricas realizadas

Recursos:
- Tamanho do binário: 34.608 bytes (limite 64 KB).
- Pico de RAM: 2.056 KB (limite 5 MB).
- Tempo por lance com orçamento padrão: 1.000 ms (limite 10 s); perft 4.865.609 nós em 0.35 s (~14 milhões de nós/s).

Perft:
- Inicial prof. 5: 4.865.609 nós.
- Kiwipete prof. 4: 4.085.603 nós.
- Posição 3 prof. 5: 674.624 nós.

Torneio (4 modelos, 1 partida por par em cada cor, 40 lances, semente 7):
- 12 partidas jogadas.
- V/E/D e winrates em centésimos:
  - modelo índice 0: 6 partidas, 0 vitórias, 6 empates, 0 derrotas, winrate 0.5000, 120 lances, 35 sub-ótimos, taxa 0.291667 por lance.
  - modelo índice 1: 6 partidas, 0/6/0, winrate 0.5000, 120 lances, 36 sub-ótimos, taxa 0.300000.
  - modelo índice 2: 6 partidas, 1/5/0, winrate 0.5833, 114 lances, 32 sub-ótimos, taxa 0.280702.
  - modelo índice 3: 6 partidas, 0/5/1, winrate 0.4167, 114 lances, 31 sub-ótimos, taxa 0.271930.
- Melhor modelo: índice 2, winrate 0.5833, taxa sub-ótima 0.280702 por lance. Desempate aplicado: winrate, depois taxa sub-ótima por lance, depois confronto direto, depois menor índice.

Modelos:
- 4 modelos gerados com semente 7 e fator de variação 0.25.
- Diferença média entre dois modelos: ~2.11 (escala absoluta do vetor), cerca de 13% da escala média da avaliação.
- A avaliação de ao menos uma posição de teste difere entre eles.

## 8. Validação contra jogador aleatório

Em 100 partidas (cores alternadas), o motor concluiu todas com 0 lances ilegais e 100 vitórias. Testado com um dos modelos gerados.

## 9. Dificuldades e retrabalho

- A posição de referência número 3 exigiu uso do FEN canônico sem dama na primeira casa; a versão inicial com dama extra produzia contagem distinta.
- A detecção de sanitizers foi adicionada como alvo opcional, evitando overhead no build padrão.
- Stockfish e Valgrind não estavam instalados; portanto as etapas que dependem deles foram puladas com aviso explícito, sem simulação de sucesso.
- A API de linhas de conjunto do HuggingFace não serviu para esses binários; usou-se leitura em streaming com Intervalo de bytes e descompressão parcial via zstd.
- Os modelos gerados inicialmente não diferiam por um fator suficiente; ajustou-se a variação e incluiu-se verificação automática de diferença mínima de 1%.
- O torneio com subprocesso é lento em partidas longas; os testes usaram limite de 40 lances.

## 10. Limitações conhecidas

- Avaliação é heurística linear; sem aprendizado supervisionado/por reforço a partir dos dados.
- Juiz interno de movimentos sub-ótimos pode ser menos preciso que Stockfish.
- A interface Kaggle usa subprocesso; deve ser chamada com caminho válido do binário.
- Abertura/variantes dependem de entradas FEN válidas.

## 11. Perguntas em aberto

1. Qual orçamento de tempo exato e em quantas cores a competição final disponibilizará o ambiente?
2. O juiz oficial da competição usará Stockfish com qual profundidade/limite?
3. A medição de memória incluirá tabelas estáticas ou apenas heap dinâmico?
4. Será exigido suporte a 100 ms por lance ou a valores menores?
5. O formato de submissão exigirá um único arquivo compactado ou fonte separada?
6. A avaliação final envolverá partidas com posições iniciais de livro ou aleatórias?
7. Haverá limite de número de modelos ou de arquivos de peso na submissão?
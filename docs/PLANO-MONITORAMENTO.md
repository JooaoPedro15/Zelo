# Diagnóstico e plano — monitoramento de crescimento (v0.3)

> Entrega de planejamento. Nenhum código foi alterado. Cada etapa ao final aguarda
> autorização antes de ser implementada.

---

## 1. Diagnóstico da versão atual

### Arquitetura existente

```
ui/          Qt Widgets — painel único: pontuação, lista de achados, detalhe, botão de limpeza
core/        C++20 puro (sem Qt, sem Windows.h): modelos, risco, confiança, pontuação,
             9 regras de análise. 100% testável sem tocar no sistema.
collectors/  Win32/WMI/Event Log, somente leitura: volumes, inicialização, eventos,
             reinício pendente, memória, Defender, discos físicos, integridade,
             catálogo de 28 locais conhecidos
scanner/     varredura Win32 (FindFirstFileExW): não atravessa reparse points,
             caminhos longos, acesso negado não derruba, cancelável
storage/     histórico de sessões em JSON, quarentena, limpeza em duas etapas
             (plan/execute), logging
```

131 testes automatizados, incluindo integração contra a máquina real.

### O que já funciona

- Análise de 9 áreas com recomendações explicáveis (evidência, risco, confiança,
  alternativa, limitações declaradas).
- Deny-list de diretórios críticos com carve-outs validados no construtor; consultada
  **duas vezes** (ao planejar e ao executar) — testada contra plano forjado.
- Catálogo de 28 locais conhecidos, cada um com "o que é / o que se perde / se volta
  sozinho". Revisado a mão, sem detecção automática de "pasta que parece cache".
- Limpeza em duas etapas com pré-visualização real (quantidade + tamanho) e confirmação.
- Quarentena com restauração e recusa de sobrescrita (após correção do bug de acento).
- Simulação (`--simular-limpeza`) que gera relatório sem tocar em nada.
- Área nunca observada é declarada, não silenciada.
- Distribuição autossuficiente (`cmake --install` + windeployqt).

### Por que a versão atual NÃO explica os 7 GB que sumiram

Cinco lacunas, em ordem de importância:

1. **Não existe varredura do disco inteiro.** O scanner só é invocado nas pastas de
   temporários e nos 28 locais do catálogo (verificado no código: `StorageScanner` só
   aparece em `temporary_files_collector` e `known_locations`). `AppData\Local\Google`
   crescer 2 GB é invisível se não estiver no catálogo.
2. **Nada é comparável no tempo.** O histórico (`history/*.json`) guarda recomendações e
   pontuação por sessão — não guarda tamanhos por pasta. Não há baseline, logo não há
   diff. A pergunta "o que cresceu desde ontem?" não tem dado para ser respondida.
3. **Sem atribuição a programas.** Nenhum mecanismo (USN, watcher, ETW) existe.
4. **Tamanho lógico apenas.** O scanner soma `nFileSizeHigh/Low`. Não mede espaço
   alocado (`GetCompressedFileSizeW`), não deduplica hard links, e — pior — **ignora
   reparse points por segurança**, o que torna arquivos do OneDrive sob demanda
   invisíveis (são reparse points). Contagem pode divergir muito do Explorer.
5. **Sem monitoramento contínuo.** O app só sabe o que vê no momento em que roda.

### Riscos e defeitos encontrados na versão atual

| # | Risco | Gravidade |
|---|---|---|
| R1 | **Itens "amarelos" são apagados direto**, sem quarentena. O modo Delete (adotado para liberar espaço de verdade) se aplica a tudo que tem botão, inclusive modelos de IA de 4 GB. A regra correta da especificação — Seguro apaga e registra, Atenção vai para quarentena — não está implementada. | Alta |
| R2 | **Quarentena vive no mesmo disco C:** que se quer liberar. A especificação pede outro disco quando disponível (D: tem ~65 GB livres). | Média |
| R3 | **Classe de bug de codificação** (caminho com acento → conversão estreita → falha silenciosa) já ocorreu **3 vezes** (spdlog, expansão de variáveis, manifesto da quarentena). Corrigidas pontualmente, mas não há auditoria sistemática de todos os pontos que abrem arquivo por string estreita, nem teste de matriz. | Alta |
| R4 | **OneDrive/arquivos de nuvem**: além de invisíveis na contagem (item 4 acima), não há distinção "só online × local × fixado", e uma limpeza futura que alcançasse essas pastas usaria exclusão comum — que **apaga da nuvem também**. Hoje nenhum local do catálogo toca OneDrive, mas a proteção é por omissão, não por design. | Média |
| R5 | **Ações não são pesquisáveis.** Limpezas vão para o log de texto; não há página de histórico/restauração na UI. Quarentena não mostra o próprio tamanho nem prazo. | Média |
| R6 | `.codex\.tmp` e similares foram classificados por inspeção pontual minha, não por confirmação do comportamento da ferramenta. A especificação exige rotular como Desconhecido o que não foi comprovado. | Média |
| R7 | `last_access` existe no modelo com flag de confiabilidade, mas o coletor nunca o preenche. | Baixa |
| R8 | O snapshot/histórico do próprio Cleaner não está excluído da futura contagem de crescimento (hoje irrelevante; vira requisito com monitoramento). | Baixa |

### O que NÃO muda

Camadas e dependências (core puro), deny-list e dupla verificação, recomendações
explicáveis, "ausência de dado ≠ dado zerado", teto de confiança, TDD contra árvores
reais. A evolução é incremental sobre isso.

---

## 2. Proposta de arquitetura incremental

Um módulo novo e dois evoluídos. Nada é reescrito.

```
src/monitor/           NOVO — snapshots, diffs, alertas, atribuição
  snapshot_store       SQLite (Qt6::Sql, driver embutido — zero dependência nova)
  snapshot_taker       varredura completa por volume → rollup por pasta até profundidade N
  diff_engine          snapshot A × snapshot B → crescimentos/reduções por pasta
  attribution          caminho → aplicativo (mapa estático + processos + USN futuro)
  watch_service        ReadDirectoryChangesW em raízes quentes, consolidação de eventos

src/collectors/profiles/   EVOLUÇÃO do catálogo — perfis por aplicativo
  cada perfil subclassifica a árvore do app em Seguro/Atenção/Perigoso/Desconhecido
  (o catálogo plano atual vira o caso trivial de perfil com uma entrada)

src/scanner/           EVOLUÇÃO — tamanho alocado, hard links, placeholders de nuvem,
                       last_access com flag de confiabilidade
```

**Por que SQLite agora, se o projeto escolheu JSON:** a decisão por JSON valia para
dezenas de sessões legíveis. Snapshots são outra espécie: centenas de milhares de linhas
(pastas × snapshots), consultas de diff e janela temporal, retenção com compactação. O
Qt já embute o driver — não entra dependência. JSON continua para sessões e decisões do
usuário; SQLite fica restrito ao `monitor/`.

### Modelo de snapshots (SQLite)

```sql
snapshot(id, taken_at, volume, total_bytes, free_bytes,
         kind /*manual|auto|pos-limpeza*/, app_version)

-- Rollup por pasta, até profundidade 4 (configurável) + qualquer pasta ≥ 50 MB.
-- Por pasta, não por arquivo: disco inteiro por arquivo estoura qualquer orçamento.
dir_entry(snapshot_id, path_id, logical_bytes, allocated_bytes, file_count)

path(id, normalized_path, owner_app /*atribuição*/, confidence)

-- Arquivos individuais só quando grandes (≥ 100 MB) ou dentro de raízes monitoradas.
big_file(snapshot_id, path_id, name, logical_bytes, allocated_bytes,
         created_at, modified_at)

change_event(id, seen_at, path_id, kind /*criado|removido|modificado*/,
             bytes_delta, source /*watcher|diff*/, process_hint, consolidated_count)

action_log(id, at, kind /*limpeza|quarentena|restauração|purga*/, path, bytes,
           mode, outcome, undo_ref)  -- histórico pesquisável (fecha R5)
```

Retenção embutida: guarda todos os snapshots de 24 h, um por dia por 30 dias, um por
semana além disso; `VACUUM` na compactação; teto configurável (padrão 200 MB) — ao
atingir, apaga os mais antigos e **avisa**. O diretório do Cleaner (banco, quarentena,
logs) é atribuído ao "Cleaner" e nunca aparece como crescimento desconhecido (fecha R8).

### Estratégia de monitoramento — três camadas, da mais barata à mais cara

1. **Snapshot sob demanda** (E1): tirado ao abrir o app, após limpeza e por botão.
   Sem processo em segundo plano. Já responde "o que cresceu desde a última vez".
2. **Watchers enquanto o app está aberto** (E5): `ReadDirectoryChangesW` nas raízes
   quentes (AppData, perfis, raízes de projeto), eventos consolidados por pasta+minuto,
   gravados como `change_event`. Responde "quando aconteceu" com hora real.
3. **USN Journal / ETW** (E9, investigação): USN dá rastreamento eficiente do volume
   inteiro, mas a leitura exige elevação — entra como capacidade opcional "monitorar com
   privilégios", nunca exigência. ETW liga arquivo a processo; custo e permissões a
   medir antes de prometer. Até lá, atribuição por processo usa apenas a lista de
   processos ativos × mapa de caminhos.

Rótulos de atribuição, sempre: **Confirmado** (evento com processo), **Altamente
provável** (caminho dentro da árvore de dados exclusiva do app), **Possivelmente
relacionado** (mapa estático), **Origem desconhecida**.

### Classificação de risco — mapeamento

| Especificação | Hoje | Mudança |
|---|---|---|
| SEGURO | Verde | igual; exclusão direta permitida, **sempre registrada** em `action_log` |
| ATENÇÃO | Amarelo | **passa a ir para quarentena**, nunca exclusão direta (fecha R1) |
| PERIGOSO | Vermelho | igual: apenas explicado |
| DESCONHECIDO | *(não existe)* | novo valor; nunca automático; é o padrão de quem não foi comprovado (fecha R6) |

Campos por item: os já existentes na `Recommendation` + tamanho alocado, flag de
confiabilidade do last_access, "será recriado?", método recomendado (apagar direto ×
quarentena × comando oficial × liberar espaço local) e possibilidade de restauração.

### Estratégia por aplicativo (perfis)

Cada perfil é uma árvore de regras revisada a mão. O que não estiver coberto pelo
perfil sai como **Desconhecido**. Resumo do que cada perfil separa:

- **VS Code** — Seguro: `Cache*`, `GPUCache`, `CachedExtensionVSIXs`, logs, crash
  reports, índices IntelliSense (`vscode-cpptools`). Atenção: versões antigas
  duplicadas de extensão (mostrando qual fica e qual sai), workspaceStorage de pastas
  que não existem mais. Perigoso: extensões atuais, `settings/keybindings/snippets`,
  workspaceStorage vivo, dados de extensões de IA.
- **Codex / Claude Code / IA** — Seguro: só o que for comprovado descartável (logs,
  downloads incompletos, atualização). **Conversas, sessões, contexto, credenciais,
  bancos locais: Perigoso, sempre**, jamais rotulados como cache. Opções de retenção
  por idade (30/60/90/180 dias) e exportação antes de remover ficam numa etapa
  própria, com confirmação individual. O que não for comprovado — incluindo
  `.codex\.tmp` — nasce Desconhecido até confirmação.
- **Navegadores** — Seguro: `Cache`, `Code Cache`, `GPUCache`, Service Worker
  CacheStorage. Perigoso: histórico, cookies, senhas, `Login Data`, `Web Data`,
  IndexedDB/LocalStorage (dados offline), extensões, sessões.
- **Adobe** — Seguro: Media Cache/Peak files (com aviso de reabertura lenta),
  temporários, cache do Creative Cloud. Atenção: previews. Perigoso: presets, plugins,
  projetos, autosaves recentes, mídia original.
- **Windows** — preferir mecanismos oficiais (motor de comandos com allowlist, já
  planejado na seção 19 do PLANEJAMENTO): limpeza de Update/Delivery Optimization,
  miniaturas, WER, despejos, Lixeira, `DISM /StartComponentCleanup` (sem ResetBase).
  Nunca WinSxS manual, nunca pagefile; hiberfil como opção separada e explicada;
  pontos de restauração com impacto declarado.
- **Nuvem (OneDrive)** — primeiro tornar visível (atributos de placeholder), depois
  distinguir só-online × local × fixado, e oferecer **"liberar espaço local"** (que
  mantém o arquivo na nuvem) como ação distinta de excluir. Exclusão que propaga para
  a nuvem sempre avisa (fecha R4).

### Modos

Análise e Simulação já existem. Seguro = só SEGURO (com a correção R1). Personalizado =
Atenção selecionável, via quarentena. Avançado e Emergência ficam para depois do motor
de comandos, porque Emergência depende de ações oficiais do Windows.

---

## 3. Etapas — pequenas, cada uma entregável e testada

| Etapa | Conteúdo | Critérios de conclusão |
|---|---|---|
| **E1. Verdade no disco** | Scanner mede tamanho alocado, detecta hard links (dedup por file ID quando nLinks > 1), reconhece placeholders de nuvem (conta como só-online, não zero), coleta last_access com flag de confiabilidade. Auditoria da classe de bug de acento: todo ponto que abre arquivo passa a usar `path` nativo; teste de matriz com diretório acentuado para cada módulo que grava. | Total do scanner ≤ 2% de divergência do Explorer numa árvore com compressão + hard links + OneDrive; testes de acento cobrindo storage, monitor e relatórios; 0 conversões estreitas de caminho restantes (grep auditável). |
| **E2. Snapshot** | `monitor/` com SQLite; varredura completa do volume → rollup por pasta (profundidade 4 + pastas ≥ 50 MB + arquivos ≥ 100 MB); snapshot ao abrir e pós-limpeza; exclusão do próprio Cleaner. | Snapshot do C: real conclui em tempo aceitável com prioridade baixa; reabrir o app mostra "último snapshot há X"; banco < 20 MB por snapshot típico. |
| **E3. Diff — "o que cresceu"** | Motor de comparação A×B; tela nova "O que cresceu" com a frase-resumo ("o C: perdeu 7,2 GB desde ontem") e a lista por pasta com atribuição estática (Possivelmente relacionado); comparação agora×último, ×ontem, ×7d, ×30d, ×par manual. | Reproduzir o caso real: criar 500 MB num diretório entre dois snapshots e o diff aponta pasta, delta e período corretos; teste de pasta removida e de pasta nova. |
| **E4. Risco de 4 níveis** | `Unknown` no enum; Atenção → quarentena obrigatória (R1); exclusão direta só para Seguro, registrada em `action_log`; página de histórico pesquisável + quarentena com tamanho próprio, prazo e restauração na UI (R5); quarentena preferindo outro disco (R2). | Teste: item amarelo executado termina na quarentena, nunca apagado; histórico lista e desfaz; quarentena em D: quando existir. |
| **E5. Watchers** | `ReadDirectoryChangesW` nas raízes quentes com consolidação; `change_event` com hora; "quando cresceu" no diff ganha horário real enquanto o app esteve aberto. | Criar arquivos num diretório monitorado gera eventos consolidados com hora; CPU do monitor imperceptível (medida); eventos param de crescer quando o disco para. |
| **E6. Perfis: VS Code + IA** | Perfis com subclassificação; detecção de versões duplicadas de extensão; conversas/sessões/credenciais Perigoso com testes explícitos ("nunca no modo seguro"); o não-comprovado nasce Desconhecido. | Testes-tabela por perfil: cada caminho protegido da especificação tem um teste que falha se ele aparecer como limpável. |
| **E7. Perfis: navegadores + Adobe** | Idem para Chrome/Edge e Adobe; detecção de app aberto antes de limpar (processo), oferta de fechar com autorização. | Limpeza com Chrome aberto pula os bloqueados e explica; senha/histórico/cookie jamais aparecem como limpáveis (teste). |
| **E8. Alertas** | Limiares configuráveis (GB/tempo, disco abaixo de X, pasta cresceu > Y); avaliados ao tirar snapshot e pelos watchers; painel mostra alerta com pastas, maiores arquivos novos e atribuição rotulada. | Simular crescimento dispara alerta com horário e pasta certos; sem falso alarme com o próprio banco do Cleaner. |
| **E9. Windows oficial + USN (investigação)** | Motor de comandos com allowlist (já especificado no PLANEJAMENTO §19) para as limpezas oficiais; investigação de USN/ETW com medição de custo e privilégio — vira proposta separada antes de qualquer implementação. | Comandos oficiais executam com confirmação, saída interpretada e registro; relatório da investigação USN/ETW com recomendação. |
| **E10. Nuvem + páginas restantes** | Estados OneDrive, "liberar espaço local", avisos de propagação; reorganização da UI nas páginas da especificação. | Arquivo só-online nunca é "excluído" para liberar espaço local; aviso de propagação testado; navegação por páginas. |

Ordem pensada para valor imediato: **E1–E3 já respondem "onde foram os 7 GB"** — o
resto refina segurança, atribuição e cobertura.

### Situação

**E1 a E10 concluídas.** Duas decisões mudaram durante a execução, ambas por pedido
ou por medição:

- **Quarentena saiu.** O usuário pediu que limpar limpasse de verdade. O que restou
  no lugar dela é o registro obrigatório em `action_log`, que não desfaz nada mas
  responde o que saiu, quando e por quê. O histórico diz isso com todas as letras:
  arquivo apagado de vez não volta por ali.
- **USN e ETW não entraram.** A medição está em
  [INVESTIGACAO-USN-ETW.md](INVESTIGACAO-USN-ETW.md). Resumo: consultar o diário
  funciona sem administrador, ler não, e o registro do USN não carrega o processo
  que escreveu — ou seja, ele resolve a metade errada do problema de atribuição.

---

## 4. Decisões que ficam registradas para sua aprovação

1. SQLite (via Qt) entra só no `monitor/`; JSON permanece no resto.
2. Atenção → quarentena passa a ser regra sem exceção (muda o comportamento atual do
   botão para itens amarelos).
3. USN/ETW é investigação com relatório antes de qualquer código.
4. Perfis novos nascem Desconhecido por padrão — cobertura cresce por revisão, não por
   palpite.

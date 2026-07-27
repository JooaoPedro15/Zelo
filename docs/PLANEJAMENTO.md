# Planejamento Técnico — Zelo (Windows Care Assistant)

> Documento de planejamento. Nenhum código de limpeza, reparo ou alteração do sistema é definido aqui para execução imediata. O MVP é somente leitura.

---

## 1. Visão do produto

**Zelo** é um aplicativo desktop para Windows que analisa, explica e orienta a manutenção do computador: armazenamento, desempenho, estabilidade, discos, inicialização, atualizações e integridade do sistema.

Ele **não é um limpador de temporários**. É um assistente que:

- mostra o que ocupa espaço e por quê;
- identifica o que pode ser limpo, movido ou revisado;
- diagnostica causas prováveis de lentidão e travamentos com evidências;
- recomenda ferramentas e comandos **oficiais** do Windows, explicando cada um;
- pede autorização antes de qualquer ação e registra tudo;
- é transparente sobre limitações: alguns problemas exigem reinstalação, driver, hardware, restauração ou formatação — e o app diz isso.

Frases proibidas no produto (nunca usar na UI ou textos): "ficará igual a novo com certeza", "substitui uma formatação", "resolverá todos os problemas", "este programa é inútil e pode ser apagado com certeza".

### Sugestões de nome

| Nome | Justificativa |
|---|---|
| **Zelo** (recomendado) | Português, curto, único, significa "cuidado/diligência". Tagline: "Windows Care Assistant". Bom para portfólio BR e diferenciado internacionalmente. |
| WinZelo | Variante com prefixo Windows; melhor para busca. |
| CareWin | Simples, autoexplicativo em inglês. |
| Vitalis | Remete a "sinais vitais" do sistema. |
| PC Health Assistant | Descritivo, genérico, fraco como marca. |

Nome **confirmado: Zelo** (decisão de 2026-07-27).

---

## 2. Problema e público-alvo

### Problema

Usuários percebem lentidão, travamentos ou disco cheio, mas não sabem: o que apagar com segurança, quais programas ainda usam, por que o C: encheu, se há corrupção do sistema, se o disco está falhando, se a causa é software ou hardware, quais comandos são seguros, e quando formatar é realmente necessário. As ferramentas existentes ou são superficiais (limpadores) ou são técnicas demais (Event Viewer, DISM, chkdsk).

### Público-alvo

- Usuários comuns de Windows sem conhecimento de terminal;
- criadores de conteúdo e editores (OBS, Premiere — arquivos grandes, caches pesados);
- jogadores (bibliotecas de jogos ocupando o C:);
- estudantes; pessoas com mais de um disco; pessoas com medo de apagar algo importante;
- quem quer evitar formatações frequentes.

---

## 3. Proposta de valor

> Analisar o computador, explicar os problemas encontrados e recomendar ações seguras de limpeza, organização, diagnóstico e reparo — sempre mostrando riscos, evidências e alternativas.

### Diferenciais

1. **Explicabilidade**: toda recomendação mostra o que foi encontrado, como foi identificado, evidências, risco, confiança, alternativas e limitações.
2. **Segurança como arquitetura**, não como aviso: allowlist de comandos, broker elevado separado, simulação antes de execução.
3. **Honestidade**: o app admite quando não sabe e quando o problema exige suporte técnico ou formatação.
4. **Local-first**: nenhum dado sai da máquina no MVP.

---

## 4. Princípios de segurança (invariantes do projeto)

Estas regras são invariantes: nenhuma versão futura pode violá-las.

1. Nenhuma alteração sem autorização explícita do usuário na ação específica.
2. Nenhum comando fora da allowlist interna revisada. Texto livre **nunca** vira comando.
3. Comandos executados sem shell (`UseShellExecute=false`, sem `cmd /c`), com executável em caminho absoluto do sistema e argumentos validados individualmente.
4. Diretórios protegidos (deny-list) jamais são alvo de limpeza/movimentação: `C:\Windows`, `System32`, `Program Files`, `Program Files (x86)`, `ProgramData`, raiz de `AppData`, raiz do perfil do usuário, raízes de unidade.
5. Sem limpadores de Registro, sem "otimizações" sem evidência, sem formatação, sem partições, sem BIOS/firmware, sem download/execução de scripts, sem drivers de fontes não oficiais.
6. Itens de risco vermelho: apenas explicados, nunca executados pelo app.
7. Ausência de dados nunca vira certeza ("sem sinal de uso" ≠ "não é usado").
8. Reparos destrutivos nunca são primeira opção nem executados em sequência automática.
9. Tudo que executa é registrado em histórico auditável local.
10. Nenhum dado do usuário sai da máquina sem opt-in explícito e explicado (pós-MVP).

---

## 5. Escopo do MVP (v0.1 — modo análise + simulação)

Totalmente seguro, majoritariamente somente leitura. Classificação: **[MVP]**.

**Coleta e análise (somente leitura):**
- Detectar discos, capacidade, espaço livre/ocupado, sistema de arquivos [MVP]
- Scanner de diretórios: maiores pastas, maiores arquivos, categorias, idade [MVP]
- Identificar temporários, caches conhecidos, Lixeira, Downloads, instaladores, ZIPs [MVP]
- Listar programas instalados (registro + MSIX) com sinais de uso [MVP]
- Listar itens de inicialização com origem, assinatura e impacto [MVP]
- Saúde básica de discos (status Windows + contadores de confiabilidade quando disponíveis) [MVP]
- Eventos recentes: falhas de aplicativos, eventos críticos, desligamentos inesperados, agrupados [MVP]
- Uso de recursos: CPU, RAM, disco, processos que mais consomem [MVP]
- Atualizações pendentes e necessidade de reinicialização [MVP]
- Estado do Windows Defender (leitura) [MVP]

**Apresentação:**
- Painel principal com pontuação de saúde explicável [MVP]
- Mapa de armazenamento (treemap simples ou barras por pasta/categoria) [MVP]
- Recomendações com risco, confiança, evidências, justificativa e alternativa [MVP]
- Central de comandos em **modo simulação**: mostra o comando que seria executado, riscos, requisitos — sem executar [MVP]
- Plano de limpeza/reparo simulado [MVP]
- Histórico de análises em arquivos JSON legíveis [MVP]
- Abrir ferramentas oficiais do Windows (somente abrir: Configurações via `ms-settings:`, Monitor de Confiabilidade, Visualizador de Eventos, Sensor de Armazenamento) [MVP]

**O MVP não faz:** apagar, mover, desinstalar, alterar Registro, executar reparos, atualizar drivers, desativar serviços, executar comandos administrativos, instalar ferramentas, baixar scripts.

---

## 6. O que fica fora do MVP

**v0.2 — versão intermediária:**
- Limpeza de itens verdes com pré-visualização e envio para Lixeira/quarentena [intermediária]
- Desativação reversível de itens de inicialização (chaves `StartupApproved`) [intermediária]
- Movimentação segura de arquivos pessoais (copiar → validar → confirmar → apagar origem) [intermediária]
- Execução autorizada de comandos de diagnóstico (DISM CheckHealth/ScanHealth, sfc /verifyonly, chkdsk leitura, powercfg relatórios) [intermediária]
- Reparos de baixo risco autorizados (sfc /scannow, DISM /RestoreHealth) [intermediária]
- Criação de ponto de restauração antes de ações relevantes [intermediária]
- Quarentena, desfazer suportado, histórico detalhado de operações [intermediária]

**Versões futuras:**
- Acompanhamento de saúde ao longo do tempo, comparação antes/depois [futura]
- Alertas de pouco espaço, detecção de crescimento anormal de pastas [futura]
- Detecção de duplicados por hash [futura]
- Integração opcional com IA (local-first, opt-in, com anonimização) [futura]
- Ferramentas de fabricantes, painel multi-PC, relatórios exportáveis, modo técnico [futura]
- Assistente de realocação de bibliotecas de jogos (orientado, via launchers oficiais) [futura]

---

## 7. Arquitetura

### Stack revisada (C++ — linguagem que o autor domina)

- **C++20 (MSVC)** — acesso direto e natural às APIs do Windows: Win32, COM (WMI, WUA, Task Scheduler) e Event Log são APIs C/C++ nativas; elimina camadas de interop.
- **Qt 6 (Widgets)** — UI desktop madura em C++: visual profissional, signals/slots, empacotamento com `windeployqt`. LGPL ok para projeto open source. *Alternativa ultra-simples avaliada*: Dear ImGui (rápido de desenvolver, estética "utilitário") — decisão na seção 31 via spike.
- **CMake + vcpkg** — build e dependências padrão de mercado (Qt via instalador oficial, ver seção 31).
- **Persistência: arquivos JSON** (`nlohmann/json`) — **sem banco de dados**. Justificativa na seção 10.
- **Logging: spdlog** — arquivo local rotativo.
- **Testes: Catch2** (via ctest).
- **`wil`** (Windows Implementation Library, Microsoft) — wrappers RAII para handles/COM, reduz verbosidade e vazamentos.

Por que trocar C#/.NET: stack anterior era adequada, mas o autor domina C++ — produtividade real e código que ele consegue defender em entrevista valem mais em portfólio pessoal. Nenhuma API necessária é perdida. Custo: COM e strings mais verbosos em C++; mitigado com `wil` e wrappers próprios finos. Objetivo declarado: **simples mas bem funcional para uso diário** — por isso a estrutura abaixo também foi simplificada.

### Estrutura simplificada: um executável, módulos internos

Antes: 7 projetos/assemblies. Agora: **um executável + bibliotecas estáticas internas** (um target CMake por módulo, mantendo testabilidade), mais o broker elevado como segundo executável na v0.2.

```
┌─────────────────────────────────────────────┐
│ ui/          (Qt Widgets)                   │  só renderiza; nunca monta comando
├─────────────────────────────────────────────┤
│ core/        modelos + regras + casos de uso│  risco, confiança, score, recomendações
│                                             │  C++ puro, sem Windows.h — 100% testável
├─────────────┬─────────────┬─────────────────┤
│ collectors/ │ scanner/    │ commands/       │  Win32/COM │ varredura │ allowlist + runner
├─────────────┴─────────────┴─────────────────┤
│ storage/     JSON: histórico + configurações│
└─────────────────────────────────────────────┘
```

Regras de dependência: `core` não inclui `Windows.h` e não conhece Qt; `ui` depende de `core`; `collectors` implementam as classes abstratas declaradas em `core/interfaces`; injeção por construtor no `main.cpp` (sem framework de DI — desnecessário nesse porte).

Regras de análise: testáveis (puras, recebem dados coletados), explicáveis (toda conclusão carrega evidências), configuráveis e versionadas (catálogo de regras com `RuleId` + versão), independentes da UI.

---

## 8. Estrutura de projetos e pastas

```
Cleaner/                          # repositório (nome do repo: zelo)
├── README.md                     # apresentação do projeto
├── CMakeLists.txt                # raiz: subdiretórios, opções, /W4 /WX
├── CMakePresets.json             # presets MSVC x64 (debug/release)
├── vcpkg.json                    # nlohmann-json, spdlog, catch2, wil
│                                 # (Qt 6 via instalador oficial + CMAKE_PREFIX_PATH — seção 31)
├── docs/
│   ├── PLANEJAMENTO.md           # este documento
│   ├── arquitetura.md            # decisões de arquitetura (ADRs curtas)
│   ├── seguranca.md              # invariantes + modelo de ameaças
│   └── comandos.md               # catálogo documentado da allowlist
├── src/
│   ├── core/                     # C++ puro, sem Windows.h e sem Qt — 100% testável
│   │   ├── models/               # Recommendation, Evidence, HealthScore, DiskInfo, ...
│   │   ├── rules/                # regras de análise (uma classe por regra, versionada)
│   │   ├── risk/                 # RiskLevel, RiskClassifier, ProtectedPaths (deny-list)
│   │   ├── confidence/           # sinais, pesos, ConfidenceCalculator
│   │   ├── scoring/              # HealthScoreCalculator, deduções explicáveis
│   │   ├── interfaces/           # classes abstratas dos coletores (IDiskCollector, ...)
│   │   └── usecases/             # QuickDiagnosis, StorageScan, RepairPlanBuilder, Simulation
│   ├── collectors/               # implementações Win32/COM das interfaces de core
│   │   ├── disks/                # GetDiskFreeSpaceEx + WMI MSFT_PhysicalDisk/Reliability
│   │   ├── programs/             # registro Uninstall (HKLM 32/64 + HKCU)
│   │   ├── startup/              # Run keys, pastas Startup, StartupApproved, Task Scheduler
│   │   ├── events/               # Event Log API (EvtQuery), agrupamento de falhas
│   │   ├── updates/              # Windows Update Agent COM (somente busca)
│   │   ├── defender/             # WMI root/Microsoft/Windows/Defender (leitura)
│   │   ├── performance/          # processos, PDH, GlobalMemoryStatusEx
│   │   └── shell/                # abrir ms-settings:, perfmon /rel (ShellExecuteW fixo)
│   ├── scanner/                  # engine de varredura, categorias, safety
│   │                             # (reparse points, caminhos longos, acesso negado, cancelamento)
│   ├── commands/                 # motor seguro: catalog (allowlist), validation,
│   │                             # execution (CreateProcessW sem shell), interpretation
│   ├── storage/                  # JSON: histórico, decisões do usuário, settings
│   │                             # (escrita atômica tmp+rename)
│   ├── ui/                       # Qt Widgets: janelas, views, cartão de recomendação,
│   │                             # selo de risco, barras de armazenamento
│   └── main.cpp                  # composição: instancia coletores e injeta em core
├── elevated/                     # (v0.2) zelo_elevated.exe — broker mínimo elevado,
│                                 # recebe CommandId + args via pipe; revalida allowlist
├── tests/
│   ├── core/                     # regras, risco, confiança, score — unitários puros
│   ├── scanner/                  # árvores temporárias (junctions, longos, permissão...)
│   ├── commands/                 # allowlist, bloqueio de injeção, interpretação
│   └── integration/              # coletores reais (label ctest: requires-windows)
└── .github/
    └── workflows/ci.yml          # build MSVC + ctest em windows-latest
```

---

## 9. Módulos principais — classes, interfaces e serviços

### Interfaces de coleta (core/interfaces — classes abstratas C++, implementadas em collectors/)

| Interface | Responsabilidade |
|---|---|
| `IDiskCollector` | Discos físicos/lógicos, capacidade, status, contadores de confiabilidade. |
| `IStorageScanner` | Varredura de diretórios com progresso, pausa e cancelamento. |
| `IInstalledProgramCollector` | Programas instalados + sinais de uso. |
| `IStartupCollector` | Itens de inicialização de todas as origens. |
| `IEventCollector` | Eventos de erro/crítico agrupados por fonte. |
| `IUpdateCollector` | Atualizações pendentes, falhas, reboot pendente. |
| `IDefenderCollector` | Estado da proteção, última verificação, ameaças registradas. |
| `IPerformanceCollector` | CPU/RAM/disco/processos no momento da análise. |

### Núcleo (core/)

| Tipo | Responsabilidade |
|---|---|
| `Recommendation` | Modelo completo da recomendação (seção 11). |
| `Evidence` | Um sinal verificável: fonte, valor, data de coleta. |
| `RiskLevel` (enum) | `Green`, `Yellow`, `Red`. |
| `RiskClassifier` | Aplica regras estáticas de risco por categoria de item/ação. |
| `ProtectedPaths` | Deny-list de diretórios críticos; única fonte de verdade. |
| `ConfidenceCalculator` | Soma ponderada de sinais → percentual + lista de motivos. |
| `HealthScoreCalculator` | Score por categoria via deduções explicáveis. |
| `AnalysisRule` (base) | Uma regra de análise: recebe dados coletados, emite 0..n recomendações. Versionada (`RuleId`, `RuleVersion`). |

### Casos de uso (core/usecases — orquestram coletores via interfaces)

| Serviço | Responsabilidade |
|---|---|
| `QuickDiagnosisUseCase` | Executa coletores leves, roda regras, monta painel. |
| `StorageScanUseCase` | Orquestra scanner + categorização + recomendações de espaço. |
| `RepairPlanBuilder` | Monta plano ordenado a partir das recomendações (simulado no MVP). |
| `SimulationService` | Gera prévia: o que seria afetado, espaço, riscos, requisitos. |
| `AnalysisSessionStore` | Persiste sessões e resultados (via `IHistoryRepository`). |

### Motor de comandos (commands/)

| Tipo | Responsabilidade |
|---|---|
| `CommandDefinition` | Registro imutável da allowlist (seção 19). |
| `CommandCatalog` | Fonte única das definições; embutida no binário, versionada. |
| `ArgumentValidator` | Valida argumentos variáveis contra padrões estritos por comando. |
| `PreconditionChecker` | Evidência exigida presente? Já executado recentemente? Disco acessível? |
| `ProcessRunner` | Executa sem shell, caminho absoluto, timeout, captura stdout/stderr. |
| `ResultInterpreter` | Código de retorno + saída → resultado em linguagem simples. |
| `ExecutionAuditor` | Grava cada execução no histórico antes e depois. |

---

## 10. Modelos e persistência (JSON — sem banco de dados)

**SQLite removido.** Motivo: volume pequeno (dezenas de sessões, centenas de recomendações), nenhuma consulta relacional complexa, e o requisito "histórico legível pelo usuário" — JSON puro atende melhor, elimina dependência, driver e migrações. Se a linha do tempo de saúde (versão futura) exigir consultas, a troca por SQLite fica localizada no módulo `storage/`.

Princípio mantido: persistir **resultados e histórico**, nunca milhões de entradas de arquivos. O scanner agrega em memória e grava apenas rollup por diretório + top-N arquivos grandes + itens categorizados.

```
%LocalAppData%\Zelo\
├── settings.json                     # preferências do usuário
├── decisions.json                    # decisões persistentes: "eu uso", "importante",
│                                     # "não recomendar", "revisar depois", ignorados
├── history/
│   └── 2026-07-27_1430_quick.json    # uma sessão por arquivo, autocontida:
│                                     #   session: início/fim, versão do app e das regras, tipo
│                                     #   storage: DirectoryRollup[], LargeFile[]
│                                     #   recommendations: Recommendation[] (com Evidence[] embutidas)
│                                     #   health: HealthSnapshot + HealthDeduction[]
│                                     #   events: EventGroup[]
│                                     #   executions: CommandExecution[] (MVP: só simulações)
├── logs/                             # spdlog rotativo
└── quarantine/                       # v0.2, manifest.json por sessão
```

Os campos de cada estrutura são os mesmos do modelo da seção 32 da especificação (Recommendation completa, Evidence com fonte/valor/data, HealthDeduction com causa visível, CommandExecution com linha completa/privilégio/saída/código, EventGroup com contagem e período).

Regras de robustez:

- **Escrita atômica**: grava em `arquivo.tmp` + `ReplaceFileW`/rename — queda de energia nunca corrompe histórico.
- **Versão de schema** no topo de cada arquivo (`"schema": 1`); leitura tolerante migra na carga.
- **Retenção**: manter últimas 50 sessões (configurável) + teto de tamanho total.
- Arquivo ilegível → renomeado para `.corrupt`, sessão nova segue; nunca derruba o app.

---

## 11. Sistema de recomendações

Pipeline: **coleta → regras → recomendações → apresentação**.

1. Coletores produzem DTOs brutos (sem julgamento).
2. Cada `AnalysisRule` recebe os DTOs e emite recomendações com evidências. Regras são puras e testáveis.
3. `RiskClassifier` e `ConfidenceCalculator` completam a recomendação.
4. UI apenas renderiza; nunca decide.

Modelo (campos, conforme especificação): identificador, título, descrição, categoria, gravidade, risco, confiança, evidências, motivos, tamanho estimado, espaço recuperável, caminhos afetados, ação recomendada, ação alternativa, ferramenta, comando associado, requer admin, requer internet, requer reinicialização, cancelável, desfazível, resultado esperado, limitações, data da análise.

Toda recomendação deve responder: *o que, como foi identificado, por que importa, qual o risco, qual a alternativa, o que o app não pode garantir.*

---

## 12. Sistema de risco e confiança

### Risco (propriedade da AÇÃO)

Classificação estática por catálogo, nunca calculada dinamicamente:

- **Verde**: temporários conhecidos, caches identificados, miniaturas, relatórios de erro antigos, logs não essenciais, tudo somente leitura.
- **Amarelo**: Downloads, arquivos grandes/antigos/duplicados, caches de programas profissionais, projetos, gravações, jogos, programas possivelmente não usados, desativar inicialização, limpeza de componentes, mover arquivos pessoais, reparos com possível reboot, chkdsk profundo, drivers.
- **Vermelho**: componentes críticos, drivers, firmware/BIOS, arquivos de sistema não identificados, Registro, serviços essenciais, partições, pastas de programas, dados pessoais, bancos de dados de apps, formatação, boot. **Apenas explicado, nunca executado.**

Qualquer caminho sob `ProtectedPaths` → automaticamente vermelho, sem exceção.

### Confiança (propriedade da CONCLUSÃO)

Independente do risco. Soma ponderada de sinais verificáveis, cada um com peso definido no catálogo de regras:

```
Exemplo — "programa possivelmente não utilizado":
+0.20 instalado há > 1 ano            (registro: InstallDate)
+0.20 sem processo/serviço ativo       (snapshot de processos)
+0.15 não está na inicialização        (coletores de startup)
+0.15 nenhum arquivo do diretório modificado em 6 meses
+0.10 sem tarefa agendada associada
+0.10 sem entrada recente em registros de execução confiáveis
+0.10 possui desinstalador oficial
−1.00 usuário marcou "eu uso"         (veto: recomendação suprimida)
Confiança = min(soma, 0.95)           # nunca exibir 100%
```

Regras: só sinais coletados de verdade entram na conta; sinal indisponível não pontua (não presume); teto de 95%; motivos exibidos são exatamente os sinais que pontuaram. `last_access_at` só é usado se `NtfsDisableLastAccessUpdate` indicar confiabilidade (campo `last_access_reliable`).

---

## 13. Pontuação de saúde

Score 0–100 geral + por categoria (armazenamento, integridade, inicialização, discos, atualizações, segurança, desempenho, estabilidade). Cada categoria parte de 100 e aplica **deduções explicáveis** (tabela `HealthDeduction`):

```
-10: disco C: com menos de 10% livre
 -6: 4 programas com falhas recorrentes nos últimos 30 dias
 -5: atualizações pendentes
 -3: 8 programas não essenciais na inicialização
```

Geral = média ponderada (pesos versionados junto com as regras; proposta inicial: armazenamento 15%, integridade 15%, discos 15%, estabilidade 15%, desempenho 15%, atualizações 10%, segurança 10%, inicialização 5%). Score é orientação, não veredito; UI nunca usa tom alarmista; toda dedução clicável mostra evidências.

---

## 14. Diagnóstico de armazenamento

### Scanner (scanner/)

- Enumeração com `FindFirstFileExW`/`FindNextFileW` + `FIND_FIRST_EX_LARGE_FETCH` (mais rápido que `std::filesystem::recursive_directory_iterator` em varredura massiva).
- **Reparse points** (symlinks, junctions): detecta `FileAttributes.ReparsePoint` e **não atravessa** — elimina loops e dupla contagem.
- **Caminhos longos**: prefixo `\\?\` + manifest `longPathAware`.
- **Acesso negado / arquivo bloqueado**: pula, conta em `skipped_count`, nunca aborta.
- **Disco removido durante análise**: trata `ERROR_NOT_READY`/`ERROR_DEVICE_NOT_CONNECTED`, encerra a varredura daquele volume com resultado parcial marcado.
- Cancelamento via `std::stop_token` (resposta < 1 s); pausa/retomada por checkpoint da fila de diretórios.
- Baixo impacto: `SetThreadPriority` abaixo do normal + `THREAD_MODE_BACKGROUND_BEGIN` (I/O de baixa prioridade); paralelismo limitado (2 threads de varredura).
- Agregação em memória: rollup por diretório + heap de top-N arquivos grandes.

### Categorização

Por extensão + caminho + heurísticas: vídeos, imagens, gravações (OBS/ShadowPlay padrões), projetos (Premiere/DaVinci), ZIPs, instaladores (`.exe`/`.msi` em Downloads), caches conhecidos (navegadores, thumbnails, `%TEMP%`, `Windows\Temp`, relatórios WER, logs), bibliotecas de jogos (Steam/Epic/Xbox por manifesto de pastas), repositórios de código (presença de `.git`), máquinas virtuais (`.vhdx`, `.vdi`), sobras de desinstalação (pasta em `Program Files` sem entrada de desinstalação correspondente — sempre amarelo/vermelho, só informativo).

### Assistente do disco C: [MVP: identificar e orientar; v0.2: mover]

Identifica conteúdo transferível (vídeos, Downloads, projetos, VMs, repositórios). Fluxo de movimentação (v0.2): verificar espaço no destino → destino acessível → criar estrutura → **copiar** → validar tamanho + hash → manter original → atualizar configuração quando houver forma oficial → **confirmar** → apagar origem (Lixeira/quarentena) → registrar → permitir recuperação. Programas instalados: nunca mover pastas; orientar movimentação oficial (Apps do Windows, launcher de jogos) ou reinstalação, abrindo a página de configurações correta.

---

## 15. Diagnóstico de desempenho

Coleta [MVP]: uso de CPU/RAM/disco no momento, top processos por recurso, contagem de itens de inicialização, espaço livre no C:, tamanho/pressão do arquivo de paginação, plano de energia ativo.

Diferenciação de cenários (perguntas guiadas na UI + evidências): lentidão constante × na inicialização × em jogos × em edição × travamentos ocasionais × congelamentos × falhas de apps × falhas do Windows.

Causas mapeadas por regras (cada uma com evidência exigida): C: quase cheio, pouca RAM disponível, paginação excessiva, excesso de inicialização, processo dominando CPU/RAM, disco 100%, verificação de antivírus em andamento, atualização em andamento, driver problemático (eventos), serviço falhando, temporários excessivos, plano de energia economizador em desktop, indícios de malware (encaminha ao Defender — o app não é antivírus).

---

## 16. Diagnóstico de estabilidade

Fontes [MVP]: Event Log API (`EvtQuery`) nos canais Application e System — Event ID 1000/1001 (falha de aplicativo/WER), 1002 (travamento), 41 (Kernel-Power, desligamento inesperado), 6008 (desligamento incorreto), 7000/7001/7031/7034 (serviços), BugCheck 1001 (tela azul); registros de confiabilidade via WMI `Win32_ReliabilityRecords` quando disponíveis.

Apresentação: nunca eventos brutos. Agrupamento por (aplicativo, módulo com falha, Event ID): *"O programa X falhou N vezes desde DATA; o componente Y aparece relacionado; erro recorrente"* + ação segura sugerida (reinstalar app, verificar atualização, procurar suporte) + acesso opcional aos detalhes técnicos completos. Botão "abrir Monitor de Confiabilidade" (`perfmon /rel`).

---

## 17. Diagnóstico de discos

Coleta [MVP]: `GetDiskFreeSpaceExW`/`GetVolumeInformationW` (capacidade, livre, filesystem), WMI `MSFT_PhysicalDisk` (tipo SSD/HDD, `HealthStatus`, `MediaType`), `MSFT_StorageReliabilityCounter` (temperatura, erros de leitura/gravação, wear — **quando o driver expõe**; NVMe/USB frequentemente não expõem), eventos disk/ntfs (ID 7, 51, 98, 153) indicando erros de I/O, papel do disco (sistema, paginação, temporários), status TRIM/otimização.

Regras de honestidade: nunca declarar "disco saudável" com base em um único indicador — exibir "nenhum problema encontrado nos indicadores disponíveis" + lista do que foi verificado e do que não estava disponível. Indício de falha física (erros SMART, eventos de I/O recorrentes) → recomendar **backup imediato**, redução de uso, ferramenta do fabricante, avaliação técnica, substituição. Nunca sugerir limpeza como conserto de hardware.

---

## 18. Diagnóstico do Windows (integridade, atualizações, drivers, memória, segurança)

- **Integridade** [MVP: detecção; v0.2: reparo]: eventos de corrupção (CBS, ESENT, ID 1001 SFC), data/resultado da última verificação (log CBS apenas leitura), pendências de manutenção. Evidência presente → recomendar `sfc` / DISM (seção 19).
- **Atualizações** [MVP]: Windows Update Agent COM (`IUpdateSearcher`, busca offline/online somente leitura): pendentes, falhas recentes (histórico), reboot pendente (chaves `RebootRequired`). Nunca instala/desinstala.
- **Drivers e dispositivos** [MVP: leitura]: WMI `Win32_PnPEntity` com `ConfigManagerErrorCode != 0` (dispositivos com alerta/erro), eventos recorrentes associados a drivers. Nunca baixa driver; orienta Windows Update/fabricante. Sem atualização automática na v1.
- **Memória RAM** [MVP: leitura; diagnóstico agendável v0.2]: total/uso/disponível, paginação, top consumidores, crescimento anômalo entre amostras (indício de vazamento — confiança baixa, requer múltiplas amostras). `mdsched.exe` apenas apresentado; agendamento (que exige reboot) só na v0.2 com confirmação dupla.
- **Segurança** [MVP: leitura]: WMI `MSFT_MpComputerStatus` — proteção ativa, definições, última verificação, ameaças registradas. O app não substitui antivírus e diz isso. Pode abrir a tela do Windows Security (`windowsdefender:`).

---

## 19. Central de comandos e reparos — allowlist inicial

MVP: todos apenas **exibidos/simulados**. v0.2: execução autorizada. Formato de cada definição no catálogo: identificador, nome amigável, executável (caminho absoluto em `%SystemRoot%\System32`), argumentos fixos, argumentos variáveis com padrão de validação, descrição, problema, fonte oficial, risco, admin, reboot, internet, timeout, pré-condições, bloqueios, interpretação de saída, códigos de retorno, cancelável, desfazível.

| Id | Comando | Problema que resolve | Evidência exigida | Risco | Admin | Reboot | Cancelável | Sucesso/Falha | Limitações |
|---|---|---|---|---|---|---|---|---|---|
| `dism.checkhealth` | `DISM /Online /Cleanup-Image /CheckHealth` | Saber se a imagem foi marcada como corrompida | Nenhuma (consulta rápida) | Verde | Sim | Não | Sim (seguro) | exit 0 + texto de estado; "repairable" → sugerir ScanHealth | Só lê flags; não varre |
| `dism.scanhealth` | `DISM /Online /Cleanup-Image /ScanHealth` | Verificar corrupção real da imagem | CheckHealth "repairable" ou eventos CBS | Verde | Sim | Não | Sim | exit 0; saída indica se há corrupção | Demorado (minutos); só leitura |
| `dism.restorehealth` | `DISM /Online /Cleanup-Image /RestoreHealth` | Reparar imagem corrompida | ScanHealth detectou corrupção | Amarelo | Sim | Raro | **Não** (não matar no meio) | exit 0 = reparado; 0x800f081f = fonte ausente → explicar opções | Pode exigir internet/Windows Update; longo; v0.2 |
| `sfc.verifyonly` | `sfc /verifyonly` | Detectar arquivos de sistema violados sem reparar | Eventos de corrupção ou suspeita | Verde | Sim | Não | Sim | exit 0 + interpretação do resumo | Log CBS difícil de ler; interpretar resumo |
| `sfc.scannow` | `sfc /scannow` | Reparar arquivos de sistema | verifyonly/eventos indicam violação | Amarelo-baixo | Sim | Às vezes | **Não** | Resumo: "não encontrou violações" / "reparou" / "não conseguiu reparar" → encadear DISM | Deve rodar após DISM se imagem corrompida; v0.2 |
| `chkdsk.scan` | `chkdsk C: /scan` | Verificação online do NTFS sem bloquear volume | Eventos ntfs/disk ou suspeita | Verde | Sim | Não | Sim | exit 0 = sem erro; 3 = erros achados → explicar `/f` com cautela | `/f` no volume de sistema exige agendamento no boot — **fora da allowlist inicial** |
| `powercfg.energy` | `powercfg /energy /output <arquivo>` | Relatório de energia/eficiência | Suspeita de plano/energia | Verde | Sim | Não | Sim | exit 0 + HTML gerado no diretório do app | Roda 60 s observando o sistema |
| `powercfg.battery` | `powercfg /batteryreport /output <arquivo>` | Saúde da bateria (notebooks) | Máquina com bateria | Verde | Não | Não | Sim | exit 0 + HTML gerado | Só notebooks |
| `defrag.analyze` | `defrag <vol> /A` | Analisar necessidade de otimização/TRIM | Disco HDD ou SSD sem TRIM recente | Verde | Sim | Não | Sim | Saída indica % fragmentação/estado | Análise apenas; otimização real fica p/ Sensor |
| `open.reliability` | `perfmon /rel` | Abrir Monitor de Confiabilidade | — | Verde | Não | Não | n/a | Ferramenta abriu | Só abre ferramenta oficial |
| `open.storagesense` | `ms-settings:storagesense` | Abrir Sensor de Armazenamento | — | Verde | Não | Não | n/a | Tela abriu | Ação fica com o usuário |
| `open.windowsupdate` | `ms-settings:windowsupdate` | Abrir Windows Update | Atualizações pendentes | Verde | Não | Não | n/a | Tela abriu | — |
| `open.security` | `windowsdefender:` | Abrir Segurança do Windows | Proteção inativa/verificação antiga | Verde | Não | Não | n/a | Tela abriu | — |
| `mdsched.schedule` | `mdsched.exe` | Diagnóstico oficial de memória | Falhas sugerindo RAM | Amarelo | Sim | **Sim** | Antes do reboot | Resultado sai no Event Log (MemoryDiagnostics-Results) após reboot | v0.2; confirmação dupla |

**Cuidado superior (fora da allowlist inicial, estudo futuro):** `chkdsk /f` (agendamento no boot; volume fica bloqueado), redefinições de rede (`netsh winsock reset` etc.), reparos de boot, `DISM /StartComponentCleanup /ResetBase` (irreversível — impede desinstalar atualizações), alterações em serviços/drivers.

Critérios gerais para recomendar qualquer comando: evidência exigida presente; não executado com sucesso nos últimos N dias (anti-duplicação via histórico); pré-condições ok (energia AC para longos, espaço livre mínimo, sem outra execução ativa — mutex global); diferenciação explícita entre *potencialmente útil / recomendado / desnecessário / não aplicável / já executado / falhou antes / requer suporte*.

---

## 20. Integração com o Windows

| Área | API (C++) | Observações |
|---|---|---|
| Discos lógicos | `GetLogicalDriveStringsW`, `GetDiskFreeSpaceExW`, `GetVolumeInformationW` | Simples e confiável |
| Discos físicos/saúde | WMI `root\Microsoft\Windows\Storage`: `MSFT_PhysicalDisk`, `MSFT_StorageReliabilityCounter` via `IWbemServices` | SMART limitado; NVMe/USB podem não expor |
| Arquivos (scanner) | `FindFirstFileExW`/`FindNextFileW` com prefixo `\\?\` | Mais rápido que `std::filesystem` em varredura massiva; long paths no manifest |
| Programas | Registro `Uninstall` (HKLM 64/32, HKCU) via `RegGetValueW` | InstallDate ausente às vezes; apps MSIX opcionais (seção 31) |
| Inicialização | Run/RunOnce keys, pastas Startup, `StartupApproved` (estado on/off), Task Scheduler COM (`ITaskService`) | Cobrir HKLM e HKCU |
| Eventos | Windows Event Log API: `EvtQuery`/`EvtNext` + XPath | Nunca despejar bruto na UI |
| Atualizações | WUA COM `IUpdateSearcher` | Somente leitura no MVP |
| Defender | WMI `root\Microsoft\Windows\Defender`: `MSFT_MpComputerStatus` | Pode falhar com antivírus de terceiros — tratar ausência |
| Desempenho | `EnumProcesses` + `GetProcessMemoryInfo`, PDH (`PdhCollectQueryData`), `GlobalMemoryStatusEx` | Amostragem pontual no MVP |
| Abrir ferramentas | `ShellExecuteW` com URIs `ms-settings:` fixas | Sem argumentos dinâmicos |

**Limitações conhecidas do Windows (documentar na UI quando afetarem resultado):** LastAccessTime frequentemente desabilitado/impreciso; não existe API confiável de "último uso de programa" (por isso confiança multi-sinal); SMART incompleto via WMI; tamanho real de apps MSIX difícil; log CBS verboso; `Win32_ReliabilityRecords` pode estar vazio se coleta desativada; leitura de alguns canais de evento e contadores exige admin — o MVP degrada graciosamente mostrando "não disponível sem elevação".

---

## 21. Permissões e elevação administrativa

**O app principal NUNCA roda elevado.** Estratégia:

1. `zelo.exe` roda como usuário comum; manifest `asInvoker`.
2. Tudo que dá para coletar sem admin, coleta sem admin. O que exigir, aparece como "requer elevação" com botão explícito.
3. **v0.2 — broker elevado** (`zelo_elevated.exe`): processo mínimo, sem UI e sem Qt, iniciado sob demanda via `ShellExecuteW` + verbo `runas` (dispara UAC). Comunicação por named pipe com handshake. O broker: recebe **apenas** `CommandId` + argumentos variáveis já validados; **revalida** tudo contra sua própria cópia embutida da allowlist (defesa em profundidade — nunca confia no chamador); executa um comando por vez; devolve saída; encerra ao terminar. Nunca recebe linha de comando pronta.
4. UAC negado = ação cancelada, registrada como "elevação recusada", sem retry automático.

---

## 22. Segurança da execução (anti-injeção)

- `CreateProcessW` direto — nunca `cmd /c`, nunca string montada por concatenação livre. Linha de comando construída por **uma única função de quoting auditada** (regras de parsing do CRT do Windows), argumento a argumento; sem shell, `& | ; > <` viram texto inerte.
- Executável sempre caminho absoluto resolvido de `%SystemRoot%\System32` (nunca PATH).
- Argumentos fixos vêm do catálogo imutável; variáveis validados por padrão estrito por parâmetro (ex.: letra de unidade `^[A-Z]:$` + unidade existente; caminho de saída apenas dentro do diretório de dados do app).
- Nenhuma UI monta comando; UI referencia `CommandId`.
- Mutex global impede execução concorrente/duplicada.
- Timeout por comando; estouro → registrado; kill somente para comandos marcados canceláveis.
- Testes dedicados de injeção (seção 25).

---

## 23. Backup, quarentena e desfazer (design v0.2)

- **Exclusões verdes**: Lixeira do Windows por padrão (recuperável nativamente) ou quarentena própria (`%ProgramData%\Zelo\Quarantine\<sessão>\`, com manifesto JSON de origem/hash/data) para itens fora do suporte da Lixeira. Expiração configurável (padrão 14 dias) com confirmação antes da purga.
- **Movimentações**: original só apagado após cópia validada (tamanho + hash) e confirmação; manifesto permite desfazer (mover de volta).
- **Ponto de restauração**: antes de reparos que alteram sistema (sfc/DISM RestoreHealth), *oferecer* criação via API oficial; nunca apresentar como backup de arquivos pessoais.
- **Desfazer**: cada operação registra plano de reversão quando existir; operações sem reversão (ex.: RestoreHealth) exibem claramente "esta ação não pode ser desfeita" antes da confirmação.

---

## 24. Logs e tratamento de erros

- **spdlog** → arquivo local (`%LocalAppData%\Zelo\logs\`, rotativo diário, retenção 30 dias) + histórico estruturado em JSON (auditoria da seção 10). Nada sai da máquina.
- Auditoria registra: análises, data/hora, versão do app e das regras, recomendações, evidências, comandos (linha completa, privilégio, saída, códigos), itens removidos/movidos, espaço recuperado, reinicializações solicitadas, ações ignoradas.
- Erros de coleta **nunca derrubam a análise**: cada coletor devolve `CollectorResult { Data, Errors, Unavailable }`; UI mostra "não foi possível analisar X" com motivo.
- Mensagens em duas camadas: linguagem simples primeiro, "detalhes técnicos" expansível (código, stack, saída bruta). Exemplo obrigatório de estilo: tradução de `0x800f081f` conforme especificação.
- Crash handler global: grava log, oferece reabrir; sessão interrompida fica marcada como incompleta no histórico.

---

## 25. Testes

- **Unitários (Catch2)**: regras de análise (entrada sintética → recomendação esperada), risco (incluindo: caminho protegido → sempre vermelho), confiança (pesos, teto 95%, veto do usuário), score (deduções), interpretação de códigos de retorno, validação de argumentos (casos maliciosos: `C:; rm`, `C:\ && calc`, path traversal, aspas, unicode homoglyph), catálogo (todo comando tem todos os campos).
- **Scanner**: árvores temporárias geradas em teste (dirs no `%TEMP%` + VHD virtual quando preciso): junctions/symlinks (não atravessa, não conta duas vezes), caminhos > 260 chars, acesso negado (ACL restritiva), arquivo em uso, cancelamento < 1 s, pausa/retomada, tamanhos batendo com valor conhecido, disco removido (VHD desmontado no meio).
- **Motor de comandos**: `ProcessRunner` testado com executável fake (echo controlado) — captura de saída, timeout, exit codes, mutex de duplicação, comando fora da allowlist rejeitado, argumento fora do padrão rejeitado.
- **Integração** (label ctest `requires-windows`): coletores reais em `windows-latest` no CI — retornam dados sem exceção, degradam sem admin.
- **VM/Sandbox** (manuais, checklist documentado): Windows Sandbox para fluxos de UI; VM com snapshot para v0.2 (limpeza real, quarentena, rollback, broker + UAC, falha no meio de movimentação). **Nunca testar operação destrutiva na máquina principal.**
- **Desempenho**: árvore sintética com 1M+ arquivos — tempo alvo e memória limitada (agregação streaming), disco quase cheio.

---

## 26. Etapas de desenvolvimento

| # | Etapa | Conteúdo |
|---|---|---|
| 0 | Fundação | Repo, projeto CMake + vcpkg + Qt configurado, targets vazios por módulo, CI (build+ctest), docs iniciais, `.clang-format`; spike Qt Widgets × ImGui |
| 1 | Domínio | Modelos, risco (+deny-list), confiança, score, primeiras regras — TDD puro, sem Windows |
| 2 | Scanner | Engine + categorização + testes de robustez (junctions, longos, permissão, cancelamento) |
| 3 | Coletores | Discos, programas, inicialização, eventos, atualizações, Defender, desempenho — todos read-only + testes de integração |
| 4 | Recomendação + score | Regras conectadas aos coletores; sessões de análise completas via testes |
| 5 | Persistência | Histórico JSON (escrita atômica, retenção), decisões do usuário, settings |
| 6 | UI | Shell Qt Widgets, dashboard, armazenamento, programas, inicialização, estabilidade, recomendações |
| 7 | Simulação + catálogo | Central de comandos exibindo allowlist, simulação de planos, auditoria de simulações |
| 8 | Release v0.1 | Polimento, empacotamento, README com screenshots, tag `v0.1.0` |
| 9+ | v0.2 | Broker elevado → comandos de diagnóstico → limpeza verde + quarentena → inicialização reversível → movimentação |

Ordem interna fixa: `core` antes de tudo; scanner antes da UI; UI por último (motor já testado sem interface).

---

## 27. Critérios de conclusão por etapa

- **0**: build CMake e `ctest` verdes no CI em `windows-latest`; janela Qt vazia abre localmente; README esqueleto; decisão Qt × ImGui registrada.
- **1**: cobertura das regras ≥ 90%; caminho protegido nunca sai de vermelho em teste de propriedade; confiança nunca exibe > 95%.
- **2**: critérios do scanner (seção 32.4) todos atendidos.
- **3**: cada coletor devolve dados reais no CI sem exceção; sem admin, degrada com `Unavailable` e motivo.
- **4**: análise completa gera recomendações com todos os campos obrigatórios preenchidos; nenhuma recomendação sem evidência.
- **5**: sessão persiste e recarrega idêntica; arquivo JSON legível a olho nu; escrita atômica sobrevive a kill no meio da gravação (teste).
- **6**: fluxo análise→painel→detalhe→evidências funcional; nenhuma ação de escrita presente.
- **7**: todo comando do catálogo exibe ficha completa; simulação nunca toca no sistema (verificado por teste que monitora ausência de escrita).
- **8**: zip gerado com `windeployqt` roda em VM limpa sem instalar nada; README com GIF/screenshot.

---

## 28. Riscos e limitações

**Técnicos**: verbosidade de COM/WMI em C++ (mitigação: wrappers RAII com `wil` + camada fina própria, escrita uma vez); distribuição Qt pesa ~30–50 MB (aceitável para app desktop; ImGui reduziria drasticamente se o spike optar por ele); SMART/WMI inconsistente entre drivers (mitigação: sempre mostrar "o que não foi possível verificar"); WUA lento em algumas máquinas (mitigação: timeout + cache); volume de eventos enorme (mitigação: janelas de tempo + agrupamento); desempenho do scanner em HDD antigos (mitigação: paralelismo baixo + progresso honesto).

**De segurança do próprio app**: broker elevado é a superfície mais sensível — allowlist duplicada e revalidada no broker, pipe com verificação de integridade do chamador, sem argumentos livres; quarentena pode acumular espaço (expiração + aviso); falso positivo de "programa não usado" (confiança multi-sinal + veto do usuário + linguagem sempre condicional).

**Do Windows**: ver seção 20. Além disso: antivírus de terceiros podem sinalizar o app (assinatura de código é melhoria futura); Controlled Folder Access pode bloquear operações da v0.2 (detectar e explicar).

---

## 29. Possíveis evoluções

Linha do tempo de saúde e comparação antes/depois [futura]; alertas de espaço e crescimento anormal [futura]; duplicados por hash [futura]; modo técnico com dados brutos [futura]; relatórios exportáveis (HTML/PDF) [futura]; IA opcional local-first com anonimização e opt-in — antes de qualquer API externa, documentar: qual API, por quê, dados enviados, custo, limitações, riscos de privacidade e alternativa local [futura]; suporte a ferramentas de fabricantes [futura]; painel multi-PC [futura].

---

## 30. Apresentação no portfólio (GitHub)

- Repo público `zelo` (ou nome escolhido), licença MIT, idioma do README: PT-BR com seção "About" em inglês (ou bilíngue).
- **README sugerido**: banner/screenshot do dashboard; uma frase de valor ("analisa e explica a saúde do seu Windows — sem promessas falsas"); GIF do fluxo análise→recomendação→evidências; seção **Princípios de segurança** em destaque (é o diferencial de engenharia); arquitetura com diagrama das camadas; stack; como rodar; roadmap v0.1→v0.2; aviso honesto de limitações.
- `docs/seguranca.md` e `docs/comandos.md` públicos — demonstram maturidade rara em projeto de portfólio.
- CI badge, releases com tag semântica, Conventional Commits em PT, issues rotuladas por módulo — mostra processo, não só código.

---

## 31. Perguntas e decisões pendentes

1. ~~Nome final~~ — **decidido: Zelo** (2026-07-27).
2. ~~Stack de linguagem~~ — **decidido: C++20** (2026-07-27; domínio do autor). ~~SQLite~~ — **decidido: JSON, sem banco** (2026-07-27).
3. **UI: Qt Widgets × Dear ImGui** — spike de 1 dia na etapa 0: janela + lista + barra de progresso em cada um. Critério: velocidade de desenvolvimento × acabamento visual. Recomendação: Qt Widgets (o repositório de skills Qt do autor sugere familiaridade; visual muito superior para portfólio).
4. **Qt via instalador oficial × vcpkg** — vcpkg compila Qt do zero (horas); instalador oficial + `CMAKE_PREFIX_PATH` é o caminho prático. Recomendação: instalador oficial.
5. **Treemap no MVP** — barras hierárquicas primeiro; treemap como custom widget depois.
6. **Idioma da UI** — PT-BR primeiro; en-US via Qt Linguist (`ts`/`qm`) desde o início ou depois?
7. **Apps MSIX/Store na listagem de programas** — registro cobre apps desktop; leitura de pacotes via C++/WinRT fica para v0.2?
8. **Distribuição** — zip com `windeployqt` no MVP; instalador (Inno Setup) e assinatura de código depois.
9. **Telemetria de crash local-only** — apenas arquivo local, sem envio; confirmar.

---

## 32. Primeiro passo prático

### 32.1 Estrutura inicial do repositório

A da seção 8, com targets CMake vazios porém ligados entre si, mais: `README.md`, `docs/PLANEJAMENTO.md` (este), `.clang-format`, `.gitignore` (C++/CMake/Qt), `CMakePresets.json` (MSVC x64, `/W4 /WX`), `vcpkg.json`, `.github/workflows/ci.yml`.

### 32.2 Ordem exata das primeiras etapas

1. Etapa 0 — fundação (repo, solution, CI).
2. Etapa 1 — `core/` com TDD: `RiskLevel` + `ProtectedPaths` + `RiskClassifier` → `Evidence`/`Recommendation` → `ConfidenceCalculator` → `HealthScoreCalculator` → 3 primeiras `AnalysisRule` (disco quase cheio; temporários excessivos; excesso de inicialização).
3. Etapa 2 — `scanner/` com TDD contra árvores temporárias.
4. Etapa 3 — coletores read-only, um por vez: discos → programas → inicialização → eventos → atualizações → Defender → desempenho.

### 32.3 O que implementar primeiro

`ProtectedPaths` + `RiskClassifier` com testes. Motivo: é o invariante de segurança que tudo depende, é puro, e define o tom do projeto desde o primeiro commit.

### 32.4 Critérios de confiabilidade do scanner

1. Total calculado de uma árvore conhecida bate exatamente com o esperado (fixture gerada em teste).
2. Em árvore real, divergência ≤ 2% contra referência (`Get-ChildItem` recursivo / WinDirStat), com diferenças explicadas (arquivos inacessíveis contados em `skipped`).
3. Junctions/symlinks: nunca atravessa, nunca conta duas vezes, nunca entra em loop (fixture com junction circular).
4. Caminhos > 260 caracteres enumerados corretamente.
5. Acesso negado e arquivo em uso: não aborta, contabiliza em `skipped_count`.
6. Cancelamento responde em < 1 s; pausa/retomada preserva resultado.
7. Duas execuções seguidas na mesma árvore estática produzem resultado idêntico.
8. 1M de arquivos sintéticos: memória estável (agregação streaming) e tempo dentro do alvo definido.
9. Remoção do volume durante a varredura: resultado parcial marcado, sem crash.

### 32.5 Primeiro commit

Conteúdo: estrutura do repositório (32.1) — projeto CMake, targets vazios, CI, docs, configs. Sem lógica ainda.

### 32.6 Mensagem de commit (Conventional Commits, PT)

```
chore: estrutura inicial do projeto e planejamento técnico

Cria o projeto CMake com os módulos core, collectors, scanner,
commands, storage e ui (Qt Widgets), testes com Catch2, pipeline
de CI e o documento de planejamento em docs/PLANEJAMENTO.md.

O MVP definido é somente leitura: análise, diagnóstico e simulação,
sem qualquer alteração no sistema.
```

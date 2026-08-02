# Recuperação máxima do disco C e reparo orientado do Windows

**Status:** desenho aprovado pelo usuário

**Data:** 2026-08-02

**Escopo inicial:** somente o disco C e a instalação atual do Windows

## 1. Contexto

O Cleaner foi criado para diagnosticar o computador, liberar espaço e orientar correções, mas a versão atual não produz o resultado percebido como necessário. Ela já removeu alguns gigabytes em execuções anteriores, porém deixa candidatos grandes sem tratamento, perde clareza sobre falhas e não mede de maneira consistente o espaço efetivamente recuperado.

Na análise realizada em 2026-08-02:

- o volume C tinha aproximadamente 222,76 GB, com apenas 6,65 GB livres, cerca de 3%;
- o AppData do usuário ocupava aproximadamente 62,5 GiB;
- `C:\Windows\Installer` ocupava aproximadamente 28,7 GiB;
- os limpadores declarados encontraram aproximadamente 18,05 GiB de candidatos;
- os maiores candidatos conhecidos incluíam cache do `uv`, temporários do Codex, caches do Chrome, shaders e arquivos temporários;
- uma amostra de CPU mostrou cerca de metade do processador explicada principalmente por Chrome e pela interface do Codex/ChatGPT naquele momento;
- não foram encontrados, nessa amostra, eventos WHEA ou falhas recentes dos controladores do SSD que comprovassem defeito físico no C.

Esses dados são um retrato, não uma promessa de espaço recuperável. Aplicativos continuam criando arquivos e o espaço deve sempre ser medido novamente antes e depois de cada execução.

O disco D permanece fora deste escopo. A corrupção lógica já observada nele será tratada em uma etapa futura e não deve bloquear a recuperação do C.

## 2. Problemas comprovados na implementação atual

1. `CleanerEngine::execute` usa exclusão direta e não trata adequadamente arquivos somente leitura. No temporário do Codex, poucos arquivos Git somente leitura concentram a maior parte do tamanho ignorado.
2. O log registra itens ignorados, mas não evidencia corretamente todos os itens que falharam.
3. A interface mostra o resultado da limpeza e chama imediatamente uma nova prévia, sobrescrevendo o resultado real.
4. Histórico e interface misturam tamanho lógico removido, estimativa e variação real de espaço livre.
5. Alguns fluxos confirmam apenas identificadores de limpadores e fazem uma nova varredura na execução. Isso permite que a seleção executada seja diferente da seleção revisada.
6. A validação de caminhos pode aceitar uma raiz redirecionada por junction ou outro reparse point e ainda sofre risco de troca entre validação e exclusão.
7. Operações longas não têm um estado de execução confiável: progresso não chega à interface, timeouts podem deixar o processo ativo e o botão pode permitir duplicação.
8. Falhas e resultados parciais podem desaparecer em caminhos assíncronos.
9. O diagnóstico atual não mede consumo de CPU por processo.
10. Uma análise indisponível pode contribuir para uma apresentação indevida de 100/100.
11. A documentação contém descrições antigas que contradizem o comportamento mais recente.

## 3. Decisões aprovadas

- O objetivo é recuperar o máximo de espaço **comprovadamente seguro**, não parar ao atingir uma porcentagem fixa.
- Dez por cento livre é somente o piso de emergência do volume, nunca o ponto de parada.
- Arquivos temporários e caches comprovadamente recriáveis serão apagados permanentemente, sem quarentena.
- Arquivos pessoais nunca serão apagados automaticamente.
- Quando um aplicativo bloquear uma limpeza, o Cleaner pedirá que o usuário o feche; nunca o encerrará sozinho.
- Aplicativos sem uso terão limiar inicial de 180 dias.
- Aplicativos pouco usados, arquivos pessoais e recursos opcionais serão ações guiadas.
- Windows Installer, WindowsApps e componentes do Windows serão auditados por item ou pacote, sem proibição genérica da área.
- Uma remoção nessas áreas deve usar o mecanismo oficial do proprietário sempre que ele existir.
- Dados inicialmente desconhecidos serão investigados; somente serão selecionáveis quando sua origem e descarte seguro forem comprovados.
- Limpeza de espaço, reparo do sistema e diagnóstico de desempenho terão métricas e resultados separados.
- A interface principal não será executada com privilégios administrativos.
- O fluxo escolhido é um pipeline unificado: diagnosticar, explicar, planejar, confirmar, aplicar e verificar.

## 4. Objetivos

### 4.1 Objetivos funcionais

1. Enumerar todos os candidatos relevantes do disco C, sem um teto arbitrário de recuperação.
2. Explicar por que cada item é ou não seguro.
3. Executar de uma vez todos os candidatos automáticos aprovados.
4. Oferecer ações guiadas para desinstalação e remoção de recursos, além de orientar a movimentação manual de arquivos pessoais.
5. Diagnosticar integridade do Windows e executar somente reparos sustentados por evidência.
6. Explicar o uso de CPU por processo e separar indícios de software, hardware e diagnóstico inconclusivo.
7. Medir e preservar um relatório fiel da execução.

### 4.2 Objetivos de segurança

1. A ação executada deve ser exatamente a ação revisada pelo usuário.
2. Nenhum redirecionamento de caminho pode ampliar a área autorizada.
3. O executor elevado deve aceitar apenas ações tipadas e revalidar todos os alvos.
4. Falhas, bloqueios, cancelamentos e timeouts nunca podem aparecer como sucesso.
5. Nenhuma recomendação genérica de desempenho pode alterar o sistema sem diagnóstico específico.

## 5. Fora de escopo

- Reparar ou mover dados do disco D nesta entrega.
- Garantir a correção de defeitos físicos por software.
- Apagar arquivos pessoais automaticamente.
- Encerrar aplicativos à força.
- Usar limpadores genéricos de registro.
- Desativar serviços, tarefas ou recursos em massa como “otimização”.
- Aplicar ajustes de timer, boot, prioridade ou rede sem um problema comprovado.
- Desfragmentar SSDs manualmente.
- Usar `DISM /ResetBase`.
- Excluir diretamente arquivos de WinSxS, WindowsApps ou Windows Installer.
- Baixar scripts ou executáveis de terceiros para reparar o Windows.

## 6. Vocabulário de decisão

### Seguro automático

Item com proprietário conhecido, função temporária ou recriável comprovada, ausência de dependências vivas e executor autorizado. Pode entrar em “Limpar tudo que é seguro”.

### Seguro por mecanismo oficial

Aplicativo, pacote ou componente que pode ser removido com segurança somente por Windows Installer, Appx, DISM, Storage ou outro desinstalador registrado.

### Ação guiada

Ação tecnicamente suportada, mas que remove uma capacidade ou dado escolhido pelo usuário, como um aplicativo pouco usado, recurso opcional ou arquivo pessoal.

### Não comprovado

Item cuja origem, dependência ou consequência não pôde ser demonstrada. Não é uma pasta “protegida por nome”, mas não pode ser apresentado como seguro nem pré-selecionado.

### Máximo seguro

Todos os itens seguros do plano confirmado são processados, mesmo depois de o volume superar 10% livre. Uma execução processa somente esse plano e termina depois de verificar cada item. A análise posterior pode descobrir outros candidatos, mas eles formam um novo plano e exigem nova confirmação; nenhum item novo ou alterado é executado implicitamente.

## 7. Arquitetura proposta

```mermaid
flowchart LR
    A["Coletores de armazenamento, aplicativos, Windows e desempenho"] --> B["Resolvedor de origem e dependências"]
    B --> C["Classificador de segurança"]
    C --> D["Plano imutável"]
    D --> E["Confirmação do usuário"]
    E --> F["Executor local sem elevação"]
    E --> G["Broker elevado mínimo"]
    F --> H["Verificação dos alvos do plano"]
    G --> H
    H --> I["Relatório persistente"]
```

### 7.1 Coordenador de diagnóstico

Executa coletores independentes e registra cobertura, duração, erro e versão de cada um. Um coletor indisponível reduz a cobertura; não produz um resultado positivo implícito.

### 7.2 Provedores de candidatos

Cada origem implementa um contrato comum de prévia, explicação, planejamento, execução e verificação. O provedor define o executor permitido, nunca uma ação textual arbitrária.

Provedores iniciais:

- caches e temporários conhecidos;
- aplicativos Win32/MSI;
- pacotes Appx/MSIX e Microsoft Store;
- componentes e resíduos de manutenção do Windows;
- restos de aplicativos removidos;
- arquivos pessoais grandes, apenas para orientação e abertura do local;
- integridade do sistema;
- desempenho, inicialização e CPU.

### 7.3 Resolvedor de origem

Cruza, conforme a categoria:

- assinatura e metadados do executável;
- produto MSI, patch e pacote local registrado;
- pacote Appx, usuário, provisionamento, framework e dependências;
- registros de desinstalação de 32 e 64 bits;
- serviços, drivers, tarefas agendadas e inicialização;
- processos e handles em uso;
- proteção de recursos do Windows e catálogos conhecidos;
- hard links, junctions, symlinks, mount points e identidade NTFS;
- atalhos, caminhos de instalação e diretórios de dados conhecidos;
- evidências de uso recente.

A idade de 180 dias é um sinal para aplicativos pouco usados, não uma prova isolada de que dados podem ser apagados.

### 7.4 Construtor de plano

Produz uma fotografia imutável da seleção. Uma nova varredura gera outro plano e exige nova confirmação.

### 7.5 Executores

- **Executor local:** ações que pertencem ao usuário e não exigem elevação.
- **Broker elevado:** processo mínimo, iniciado somente para uma ação autorizada, sem interface e sem aceitar comandos de shell livres.
- **Executores gerenciados:** adaptadores para MSI, Appx, DISM, Storage, SFC e CHKDSK.

Uma `ExecutionOperation` corresponde a uma confirmação do usuário e a um manifesto. Quando houver ações elevadas, uma única instância do broker é iniciada para essa operação e executa sequencialmente apenas as ações tipadas contidas no manifesto. O nonce autoriza somente o estabelecimento dessa operação e é consumido na conexão; cada ação possui ID estável e estado persistido que impede repetição. Uma operação exclusivamente local não inicia o broker.

O broker se comunica apenas por IPC local. Ele autentica o processo cliente, o usuário e a sessão, recebe o manifesto canônico com nonce e validade e revalida independentemente cada ação tipada. Um hash enviado pela interface não constitui autorização.

### 7.6 Verificador

Confirma o estado de cada alvo, mede o volume, repete o diagnóstico relevante e produz `sucesso`, `parcial`, `falha`, `cancelado`, `aguardando reinício` ou `ainda executando`.

## 8. Modelo de dados

### 8.1 Candidato

Cada candidato contém pelo menos:

- identificador estável;
- provedor e categoria;
- alvo tipado como `FileTarget` ou `ManagedAction`;
- caminho final resolvido, volume e identidade do arquivo quando aplicável;
- proprietário e evidências de origem;
- evidências de segurança e dependências encontradas;
- tamanho lógico estimado e incerteza;
- risco e consequência;
- método de execução permitido;
- necessidade de administrador, fechamento de aplicativo ou reinicialização;
- data e evidência de último uso;
- token de frescor para revalidação;
- estado de seleção.

`FileTarget` identifica um arquivo pelo caminho final, volume, file ID, metadados relevantes e identidade observada no momento do plano.

Diretórios são apenas agrupamentos de apresentação. Antes da confirmação, todo conteúdo elegível é expandido em `FileTarget`s individuais. A execução nunca faz exclusão recursiva aberta nem inclui filhos criados depois do plano. Um diretório pode ser removido pelo mesmo mecanismo seguro somente se estiver vazio e conservar a identidade confirmada.

`ManagedAction` registra provedor, operação fechada e escopo exato:

- MSI: `ProductCode`, contexto de instalação e SID quando aplicável;
- Appx instalado: `PackageFullName` e SID alvo;
- Appx provisionado: `PackageName` de provisionamento, como ação distinta;
- DISM/SFC/CHKDSK/Storage: identificador de receita pertencente ao catálogo fechado;
- desinstalador Win32: executável, argumentos, identidade do arquivo e registro de origem.

Uma `UninstallString` nunca é entregue ao shell como texto livre. Executável, argumentos, assinatura/identidade e registro devem continuar iguais aos do plano.

### 8.2 Plano

O plano contém:

- identificador e data;
- identidade do volume C;
- espaço livre inicial;
- versão dos coletores e cobertura;
- candidatos exatos e respectivas identidades;
- totais automáticos, guiados e não comprovados;
- manifesto canônico de uma `ExecutionOperation`, vinculado a usuário, sessão, volume, validade e nonce de conexão de uso único;
- consentimentos e consequências apresentados.

Hash ou assinatura fornecida apenas pela interface não concede autoridade. O broker autentica o canal e o cliente, consome o nonce da operação uma única vez e aplica novamente catálogo, escopo e políticas de segurança. Cada ID de ação só pode avançar uma vez a partir do estado persistido; reconciliação nunca repete automaticamente uma ação de resultado desconhecido.

### 8.3 Resultado

O histórico separa:

- bytes lógicos removidos;
- variação observada no espaço livre do volume;
- ruído ou incerteza entre as medições;
- arquivos, pacotes e aplicativos processados;
- sucessos, falhas, bloqueios e itens alterados desde o plano;
- reparos tentados e verificação correspondente;
- reinicialização pendente;
- cobertura final do diagnóstico.

Antes da primeira mutação, o histórico persiste o identificador da operação, o plano autorizado e o estado de cada ação. Também mantém timestamps, PID/identidade do broker e dados suficientes para reconciliar uma operação depois de queda da interface ou reinício.

A persistência existente em SQLite deve receber migração compatível com históricos antigos.

## 9. Fluxo de limpeza

1. Medir o volume C e registrar a fotografia inicial.
2. Executar todos os coletores e indicar qualquer cobertura ausente.
3. Resolver origem, dependências, riscos e bloqueadores.
4. Construir o plano imutável.
5. Mostrar separadamente:
   - espaço seguro recuperável agora;
   - potencial de ações guiadas;
   - dados não comprovados;
   - problemas de integridade e desempenho.
6. Pedir fechamento de aplicativos quando necessário e oferecer “Já fechei — verificar novamente”.
7. Exibir confirmação com alvos, totais e consequências.
8. Revalidar a identidade de cada alvo imediatamente antes da ação.
9. Executar ações locais e elevadas uma por vez por categoria.
10. Medir o espaço depois da execução, preservar o resultado e verificar automaticamente somente os alvos e diagnósticos afetados pelo plano. Uma análise completa para descobrir novos candidatos continua sendo uma ação explícita e sempre gera outro plano.

O piso de 10% apenas controla a severidade visual do alerta. Ele não participa da condição de parada da limpeza.

## 10. Regras por área

### 10.1 Caches e temporários de aplicativos

Podem ser automáticos quando o caminho pertence a uma especificação conhecida, o conteúdo é recriável e não há processo usando os arquivos.

Para o temporário do Codex:

- o provedor deve marcá-lo como dependente do fechamento do aplicativo;
- arquivos somente leitura não podem ser ignorados silenciosamente;
- o atributo somente leitura só pode ser alterado pelo handle já validado, depois de confirmar raiz temporária conhecida, ausência de handles, inatividade e identidade do arquivo; se a exclusão falhar, o atributo original deve ser restaurado;
- arquivos novos ou alterados após a confirmação são ignorados;
- falhas permanecem no resultado com motivo e tamanho.

### 10.2 Windows Installer

`C:\Windows\Installer` é auditável, mas não é uma pasta de exclusão direta. O cache contém arquivos necessários para reparar, atualizar e remover produtos MSI.

O provedor deve:

- enumerar produtos e patches em todos os contextos relevantes;
- relacionar arquivos locais com produto, patch e estado de instalação;
- apresentar aplicativos pouco usados somente como ação guiada de desinstalação;
- invocar Windows Installer ou o desinstalador registrado;
- medir o espaço liberado pelo conjunto da desinstalação;
- diagnosticar cache inconsistente sem prometer que um arquivo sem referência aparente é seguro para exclusão direta.

Se não existir um mecanismo suportado que comprove a remoção, o item permanece não comprovado.

### 10.3 WindowsApps e pacotes Appx/MSIX

O provedor deve enumerar separadamente pacotes instalados por usuário e pacotes provisionados, além de dependências e frameworks. Remover provisionamento não equivale a remover instalações existentes. Aplicativos opcionais e pouco usados podem ser oferecidos como ações guiadas por `Remove-AppxPackage` ou DISM, com SID e escopo explícitos.

Devem ser excluídos da seleção automática:

- Microsoft Store;
- shell e experiência essencial do Windows;
- segurança do Windows;
- WebView e frameworks exigidos por pacotes instalados;
- dependências ativas;
- pacotes cujo impacto não pôde ser determinado.

Pastas dentro de WindowsApps nunca são apagadas diretamente.

### 10.4 Componentes do Windows

O tamanho real deve ser obtido por `DISM /Online /Cleanup-Image /AnalyzeComponentStore`, que considera hard links.

Quando o Windows recomendar limpeza, o aplicativo pode oferecer `StartComponentCleanup` como `ManagedCleanupRecipe` fechada. Ela pertence à limpeza gerenciada, não ao reparo: registra a estimativa fornecida pelo DISM quando disponível, marca bytes lógicos como não informados e mede separadamente a variação real do volume com sua incerteza. Também pode usar categorias oficiais de limpeza do Windows Update e apresentar recursos opcionais não usados como ações guiadas.

`Windows.old` é sempre uma ação guiada e só pode ser removido com confirmação explícita de que a reversão da versão anterior será perdida.

Não usar `ResetBase` e não apagar WinSxS manualmente.

### 10.5 Restos de aplicativos removidos

Um diretório só é automático quando:

- não existe produto, pacote, serviço, tarefa, driver ou executável instalado que o reivindique;
- não há processo ou handle ativo;
- a estrutura corresponde a cache, log, crash dump ou temporário conhecido;
- não contém arquivos pessoais ou configuração que o usuário possa querer preservar;
- a raiz e todos os ancestrais passam pela validação de caminhos.

Configurações, bancos locais, projetos, saves e conteúdo criado pelo usuário são guiados ou não comprovados.

### 10.6 Arquivos de origem desconhecida

O Cleaner deve tentar resolver a origem e explicar as evidências consultadas. Se a investigação continuar inconclusiva, o item é exibido com tamanho e motivo, mas não recebe ação de exclusão segura.

## 11. Segurança de caminhos e elevação

1. Resolver pastas conhecidas por APIs do Windows, sem confiar apenas em variáveis de ambiente.
2. Abrir a raiz e o alvo com flags adequadas para inspeção de reparse points.
3. Rejeitar reparse points na raiz permitida ou em seus ancestrais para exclusões automáticas por arquivo. Um provedor gerenciado só pode atravessar redirecionamentos quando sua API oficial não opera por caminho e valida o objeto por identidade própria.
4. Obter caminho final, volume e identidade do arquivo pelo handle.
5. Aplicar regras de exclusão ao destino final, não apenas ao caminho digitado.
6. Vincular essa identidade ao plano confirmado.
7. Manter aberto, da validação até a disposição, o mesmo handle criado sem seguir reparse points e revalidar sua identidade no executor ou broker.
8. Excluir pelo handle validado ou por API gerenciada que opere sobre a identidade tipada. Não existe fallback de exclusão por caminho textual; se o método seguro não estiver disponível, o item é ignorado.
9. Rejeitar qualquer alvo novo, alterado, movido ou fora do volume planejado.
10. Registrar o motivo sem tentar “contornar” a proteção.

O broker aceita uma lista fechada de tipos de ação e parâmetros validados. Ele não recebe linhas de comando arbitrárias da interface.

O canal IPC é exclusivamente local, com ACL ligada ao SID de logon e à sessão. O broker valida o PID, token, sessão e identidade do executável cliente, além do volume e validade do manifesto. Cada operação usa uma conexão e um nonce consumido uma única vez para impedir replay. As ações dessa operação são sequenciais, possuem IDs persistidos e não podem ser repetidas. O broker não confia no hash, caminho ou decisão de segurança enviados pela interface: ele repete as validações aplicáveis antes da mutação.

## 12. Diagnóstico e reparo do Windows

### 12.1 Diagnóstico

O Cleaner pode coletar:

- estado e análise do armazenamento de componentes;
- integridade da imagem do Windows;
- verificação de arquivos protegidos;
- `chkdsk C: /scan` para análise online do NTFS;
- reinicialização ou atualização pendente;
- falhas recorrentes em eventos, serviços e tarefas;
- estado do Windows Update, Microsoft Store e Windows Security;
- integridade declarada do disco e eventos WHEA/armazenamento;
- inicialização e atividade em segundo plano.

### 12.2 Receitas gerenciadas e reparos condicionais

Toda ação oficial é uma `ManagedRecipe` fechada de um destes tipos:

- `DiagnosticRecipe`: somente coleta estado e não modifica o sistema;
- `ManagedCleanupRecipe`: remove conteúdo pelo mecanismo oficial e contribui para as métricas de limpeza;
- `RepairRecipe`: corrige integridade ou funcionamento e não conta como espaço liberado.

Cada receita define:

- evidência que a habilita e versões do Windows suportadas;
- API ou executável e argumentos fixos;
- elevação, rede ou fonte de reparo necessárias;
- efeitos e confirmação apresentada;
- política de timeout, cancelamento e reinicialização;
- códigos de saída aceitos;
- verificação posterior obrigatória.

O catálogo inicial pode conter somente receitas completamente especificadas para:

- analisar o armazenamento de componentes (`DiagnosticRecipe`);
- limpar o armazenamento de componentes (`ManagedCleanupRecipe`);
- verificar a imagem do Windows e os arquivos protegidos (`DiagnosticRecipe`);
- restaurar a imagem do Windows e reparar arquivos protegidos (`RepairRecipe`);
- analisar o NTFS do C (`DiagnosticRecipe`) e agendar correção quando houver evidência (`RepairRecipe`);
- redefinir o cache da Microsoft Store quando o erro diagnosticado for especificamente desse cache (`RepairRecipe`).

Se houver corrupção da imagem ou de arquivos protegidos, a cadeia autorizada executa DISM com `RestoreHealth` e depois SFC com `scannow`, seguindo a ordem suportada pela Microsoft. `StartComponentCleanup` exige a análise correspondente. CHKDSK com correção exige erro detectado e confirmação de reinicialização/bloqueio.

Reparos do Windows Update, Store ou qualquer outro subsistema que ainda não possuam receita fechada permanecem apenas como orientação e abertura da ferramenta oficial. Nenhuma sequência genérica de “reset” é montada dinamicamente.

Reparos não contam como espaço liberado. Limpezas gerenciadas registram estimativa, bytes lógicos quando o provedor os fornecer e delta observado do volume como métricas distintas. Um comando com código zero também não basta: o estado final deve ser verificado.

## 13. Diagnóstico de CPU e distinção entre software e hardware

O coletor de desempenho deve medir, por uma janela definida:

- CPU total e por processo;
- memória, leitura e escrita por processo;
- frequência e limitação observável;
- processos de inicialização e segundo plano;
- eventos WHEA e de armazenamento;
- saúde declarada dos discos;
- temperatura somente quando uma fonte confiável estiver disponível.

O resultado possui três classes:

- **Provável software:** a carga é explicada por processos, inicialização, serviço ou falha reproduzível.
- **Possível hardware:** há evidências como WHEA, throttling, falhas de disco/controlador ou comportamento não explicado acompanhado de sinal físico.
- **Inconclusivo:** faltam dados confiáveis; o aplicativo diz o que não conseguiu medir.

O Cleaner mostra os maiores responsáveis, a duração da amostra e a cobertura. Ele não diagnostica defeito de hardware apenas por um pico de CPU.

## 14. Interface

### 14.1 Resumo do C

Mostrar:

- total, usado e livre;
- piso emergencial de 10%;
- seguro recuperável agora;
- potencial por ações guiadas;
- problemas do Windows;
- cobertura do diagnóstico.

### 14.2 Grupos de resultado

1. Limpeza segura.
2. Aplicativos sem uso.
3. Restos de aplicativos removidos.
4. Windows e sistema.
5. Desempenho e CPU.

Cada item mostra tamanho, origem, evidências, consequência, executor, bloqueadores e necessidade de reinício.

### 14.3 Ações principais

- **Limpar tudo que é seguro**
- **Revisar ações guiadas**
- **Corrigir problemas encontrados**
- **Analisar novamente**

### 14.4 Estados

```text
Ocioso
  -> Diagnosticando
  -> Plano pronto
  -> Confirmando
  -> Executando
  -> Verificando
  -> Sucesso | Parcial | Falha | Cancelado
```

`Executando`, `Cancelamento solicitado`, `Ainda executando` e `Aguardando reinício` são estados não terminais. `Cancelado` só é registrado depois de confirmar que a operação terminou.

O resultado final permanece visível. A verificação automática pós-execução consulta somente os alvos e diagnósticos afetados pelo plano. Uma nova análise completa é uma ação explícita e não sobrescreve silenciosamente o relatório anterior.

### 14.5 Acessibilidade

- nomes acessíveis e atalhos para ações essenciais;
- navegação completa por teclado;
- labels associados aos controles;
- contraste suficiente;
- estado não comunicado somente por cor;
- detalhes essenciais visíveis sem depender apenas de tooltip.

## 15. Progresso, cancelamento e erros

- O progresso é publicado por etapa e por ação.
- Somente uma instância de cada operação pode estar ativa.
- Antes da primeira mutação, persistir a operação, o manifesto do plano e o estado individual das ações.
- Timeout não encerra semanticamente uma operação ainda viva; o estado permanece “ainda executando” até confirmação.
- Cancelamento é cooperativo e nunca interrompe uma operação do Windows em um ponto inseguro.
- “Cancelamento solicitado” permanece não terminal até o processo ou broker confirmar o término.
- Falha individual produz resultado parcial e não apaga os sucessos anteriores.
- O usuário recebe motivo, alvo, tamanho e ação possível para cada falha.
- O broker persiste PID/identidade, progresso e resultado independentemente da janela.
- Na abertura seguinte, o Cleaner reconcilia operações pendentes e nunca as repete automaticamente.
- Ações que dependem de reinício ficam pendentes e são verificadas na próxima abertura antes de receber estado terminal.

## 16. Estratégia de testes

O desenvolvimento seguirá testes antes da implementação de cada comportamento.

### 16.1 Testes unitários

- classificação e evidências;
- cálculo do máximo seguro sem parada em 10%;
- imutabilidade do plano;
- separação de métricas;
- regras de MSI, Appx, componentes, restos e arquivos pessoais;
- estados de execução e resultados parciais;
- classificação de CPU por evidência e cobertura.

### 16.2 Testes de segurança

- raiz permitida que é junction;
- ancestral redirecionado;
- symlink/mount point no alvo;
- troca do arquivo depois da confirmação;
- arquivo novo entre prévia e execução;
- mudança de volume ou identidade;
- tentativa de enviar ação não autorizada ao broker;
- cliente falso ou de outra sessão tentando usar o broker;
- replay de nonce, plano expirado ou volume divergente;
- segunda execução de um ID de ação já consumido dentro da mesma operação;
- arquivo somente leitura, bloqueado e alterado;
- filho criado dentro de diretório depois da confirmação;
- caminho final que cai em área não autorizada.

### 16.3 Testes de integração

- usar diretórios temporários controlados e provedores falsos;
- simular MSI, Appx, DISM, SFC e CHKDSK;
- nunca executar reparos reais do Windows em testes automatizados;
- validar migração do SQLite e leitura do histórico antigo;
- comprovar que resultado não é sobrescrito por prévia;
- comprovar que timeout não permite execução duplicada;
- simular queda antes, durante e depois da primeira mutação e reconciliar o estado sem repetir ações.

### 16.4 Testes de interface

- transições completas de estado;
- progresso, cancelamento, parcial e reinício;
- confirmação vinculada ao plano;
- navegação por teclado e nomes acessíveis;
- cobertura incompleta não gera 100/100.

### 16.5 Validação manual no computador do usuário

1. Gerar relatório de prévia sem mutação.
2. Conferir candidatos automáticos, guiados e não comprovados.
3. Medir espaço livre inicial.
4. Pedir fechamento dos aplicativos necessários.
5. Executar somente o plano confirmado.
6. Medir a variação real e verificar os alvos afetados.
7. Executar reparos aprovados separadamente.
8. Verificar novamente os problemas diagnosticados.
9. Entregar relatório com limitações e pendências.

Uma análise completa posterior é iniciada pelo usuário e, se encontrar novos candidatos, produz outro plano. Nesta entrega, a orientação sobre arquivos pessoais abre o local e explica opções de destino; o Cleaner não move nem exclui esses arquivos.

Não haverá quarentena. O relatório preserva a auditoria, não os arquivos excluídos.

## 17. Critérios de aceitação

1. A limpeza não termina ao alcançar 10% livre.
2. Todo item automático do plano confirmado recebe resultado individual. Filhos ou candidatos descobertos ou alterados depois da confirmação aparecem somente em um novo plano e nunca são executados sem nova confirmação.
3. O resultado mostra estimativa, bytes lógicos e variação real separadamente.
4. Arquivos adicionados ou alterados depois da confirmação não são apagados.
5. Arquivos somente leitura e bloqueados são tratados ou explicados, nunca ignorados silenciosamente.
6. Nenhum arquivo pessoal é removido automaticamente.
7. Windows Installer, WindowsApps e WinSxS não sofrem exclusão direta.
8. Aplicativos e componentes são removidos pelo mecanismo oficial.
9. O broker rejeita redirecionamentos, trocas, cliente falso, replay, manifesto expirado e ações fora do plano.
10. A interface não permite duas operações iguais simultâneas.
11. O resultado permanece visível após a execução.
12. Reparos possuem receita fechada, diagnóstico anterior e verificação posterior.
13. Uma análise indisponível reduz a cobertura e impede nota completa.
14. O relatório distingue provável software, possível hardware e inconclusivo.
15. Testes automatizados não alteram o Windows real.
16. Queda da interface, timeout e reinicialização preservam uma operação não terminal e não causam repetição automática.
17. `StartComponentCleanup` aparece como limpeza gerenciada e contribui apenas para as métricas de limpeza suportadas pelo provedor, nunca para o total de reparos.

## 18. Sequência de entrega

1. Corrigir métricas, resultado persistente e estados de execução.
2. Introduzir plano imutável e segurança de caminhos.
3. Criar broker elevado mínimo e executores tipados.
4. Migrar limpadores atuais para provedores com evidências.
5. Adicionar MSI, Appx, componentes do Windows e restos de aplicativos.
6. Adicionar diagnóstico e reparo do Windows.
7. Adicionar CPU por processo e classificação software/hardware.
8. Concluir interface, acessibilidade e validação real do C.

Cada etapa deve manter o aplicativo compilável e testável. A limpeza real só é habilitada para um provedor depois de seus testes de elegibilidade, segurança e verificação.

## 19. Riscos e mitigação

| Risco | Mitigação |
|---|---|
| Classificar resíduo como órfão por registro incompleto | Exigir múltiplas evidências e mecanismo oficial; manter como não comprovado quando houver dúvida |
| Espaço livre variar por atividade paralela | Registrar tamanho lógico, delta do volume e incerteza separadamente |
| Arquivo mudar entre análise e exclusão | Plano imutável, identidade NTFS e revalidação no executor |
| Aplicativo recriar cache durante limpeza | Solicitar fechamento, verificar handles e repetir análise |
| Comando do Windows durar muito | Estado persistente, progresso, cancelamento seguro e proibição de duplicação |
| Reparar sem necessidade | Diagnóstico obrigatório, confirmação por ação e verificação posterior |
| Confundir carga de software com defeito físico | Janela de amostragem, evidências explícitas e resultado inconclusivo quando necessário |

## 20. Referências oficiais

- [Free up drive space in Windows](https://support.microsoft.com/en-US/Windows/Experience/Storage-FileManagement/free-up-drive-space-in-windows)
- [Restore missing Windows Installer cache files](https://learn.microsoft.com/en-us/troubleshoot/windows-client/application-management/missing-windows-installer-cache)
- [Modern, Inbox, and Microsoft Store Apps troubleshooting guidance](https://learn.microsoft.com/en-us/troubleshoot/windows-client/shell-experience/modern-inbox-store-apps-troubleshooting-guidance)
- [Determine the actual size of the WinSxS folder](https://learn.microsoft.com/en-in/windows-hardware/manufacture/desktop/determine-the-actual-size-of-the-winsxs-folder?view=windows-11)
- [Clean up the WinSxS folder](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/clean-up-the-winsxs-folder?view=windows-11)
- [Use DISM and SFC to repair Windows](https://support.microsoft.com/en-us/windows/experience/backup-recovery/use-the-system-file-checker-tool-to-repair-missing-or-corrupted-system-files)
- [CHKDSK command reference](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/chkdsk)

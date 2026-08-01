# Investigação: USN Journal e ETW para atribuição de arquivos

Etapa 9 do plano de monitoramento. O plano registrou que USN/ETW entrariam como
**investigação com relatório antes de qualquer código**. Este é o relatório.

A pergunta é uma só: **o Zelo pode dizer com segurança qual programa criou um
arquivo?** Hoje ele responde com rótulos de confiança (Confirmado / Altamente
provável / Possivelmente relacionado / Origem desconhecida) apoiados em mapa
estático de caminhos e lista de processos ativos. USN e ETW são os dois caminhos
que poderiam elevar isso a "Confirmado" de verdade.

---

## 1. O que foi medido, e o que não foi

Tudo abaixo foi medido **neste computador**, em 01/08/2026, com sondas escritas
para esta investigação (`usn_probe.cpp` e `usn_probe2.cpp`, descartadas depois —
o que importa são os números). A sessão **não** tinha privilégio de
administrador, que é exatamente a condição em que o Zelo roda no dia a dia.

| Tentativa | Resultado |
|---|---|
| Abrir `\\.\C:` com acesso `0` | **Funciona** |
| `FSCTL_QUERY_USN_JOURNAL` nessa alça | Falha — erro 1 (função inválida) |
| Abrir `\\.\C:` com `GENERIC_READ` | Falha — **erro 5 (acesso negado)** |
| Abrir `C:\` como diretório, `FILE_READ_ATTRIBUTES` | **Funciona** |
| `FSCTL_QUERY_USN_JOURNAL` nessa alça | **Funciona** |
| `FSCTL_READ_USN_JOURNAL` nessa alça | Falha — **erro 5 (acesso negado)** |

Estado do diário no momento da medição:

- Faixa em uso: USN 105.318.973.440 a 105.353.348.424 — **32,8 MB** de registros.
- Tamanho máximo configurado: **32 MB** (`fsutil usn queryjournal C:`).
- Rastreamento de intervalo de gravação: **desabilitado**.

**Não foi medido:** o tempo de leitura do diário com privilégio elevado, e o
custo de resolver `FileReferenceNumber` para caminho. Ambos dependem de
administrador, que esta investigação não assumiu. Ficam registrados como
desconhecidos em vez de estimados — o projeto já decidiu que palpite não vira
número na tela.

---

## 2. Leitura do que foi medido

### 2.1 Consultar é livre; ler exige administrador

Essa é a descoberta que muda o desenho. A distinção não é entre "o diário existe"
e "o diário não existe" — é entre **saber que ele existe** e **poder lê-lo**.

Consultar funciona sem privilégio: o Zelo pode dizer ao usuário que o diário
está ligado, qual é a faixa e quanto ele guarda. Ler os registros não funciona:
`FSCTL_READ_USN_JOURNAL` devolve acesso negado.

Isso confirma o que o plano supôs, mas por um caminho diferente do esperado. A
suposição era que abrir o volume seria a barreira. Abrir o volume passa; o que
não passa é o FSCTL de leitura. Se a implementação tivesse sido escrita em cima
da suposição, ela abriria a alça com sucesso, pareceria funcionar na inicialização
e falharia no primeiro registro — o tipo de erro que aparece só em produção.

### 2.2 A janela do diário é curta

32 MB de diário, com registros de tamanho variável em torno de 100 bytes, dá algo
como **300 mil registros**. Em uma máquina com compilação, navegador e
ferramentas de IA escrevendo o tempo todo, isso cobre horas — não dias.

Consequência direta: **o USN não é um histórico**. Ele responde "o que mudou
desde a última vez que olhei", desde que a última vez tenha sido recente. Se o
Zelo ficar dois dias fechado, o começo da faixa já rolou para fora e a resposta
honesta passa a ser "não sei o que aconteceu nesse intervalo".

Um monitor que promete explicar crescimento e depende só do USN teria buracos
silenciosos. Os retratos (E1–E3) não têm esse problema: eles medem o estado, não
o fluxo, então nunca perdem um intervalo — só perdem detalhe.

### 2.3 O USN não sabe quem escreveu

Ponto decisivo, e independente de privilégio: o registro USN traz o nome do
arquivo, a referência do pai, o motivo da mudança e a hora. **Não traz
identificador de processo.**

Ou seja: mesmo com administrador, mesmo com o diário inteiro lido, o USN não
sustenta o rótulo "Confirmado". Ele diria "este arquivo apareceu às 14h32",
nunca "o Chrome criou este arquivo". Para a atribuição — que é o motivo pelo qual
o USN entrou no plano — ele resolve a metade errada do problema.

### 2.4 ETW resolve a atribuição, e cobra caro por isso

O ETW com os provedores de arquivo e processo em modo núcleo é o único caminho
que liga escrita a processo. Ele também **exige administrador**
(`SeSystemProfilePrivilege` para a sessão do kernel logger), e a sessão precisa
ficar de pé enquanto o Zelo estiver observando.

Isso não foi medido aqui porque a medição exigiria elevar a sessão, o que
mudaria a condição que a investigação queria testar. Fica como desconhecido
declarado.

O que já dá para afirmar sem medir: o volume de eventos de I/O de arquivo em uma
máquina de trabalho é alto o bastante para que a própria coleta apareça no
consumo. Um programa que promete cuidar do computador não pode ser o processo que
mais consome nele.

---

## 3. Recomendação

**Não implementar USN nem ETW agora.** Três razões, em ordem de peso:

1. **O USN não responde a pergunta que motivou incluí-lo.** Sem PID no registro,
   ele não eleva atribuição alguma para "Confirmado". O ganho seria detectar
   mudança mais barato que um retrato — e o retrato já custa pouco o bastante
   para ser pedido pelo usuário.

2. **Ambos exigem administrador.** O Zelo hoje faz seu trabalho sem elevação.
   Trocar isso por atribuição mais precisa é trocar uma garantia concreta por uma
   melhoria de rótulo. Exigir administrador para abrir o programa é a mudança de
   comportamento mais visível que este projeto poderia fazer, e ela não se paga.

3. **A cobertura atual já explica o crescimento.** Retrato + comparação + pastas
   observadas respondem "onde foi parar o espaço" com hora e pasta. O que falta é
   "qual programa", e para essa lacuna existe a resposta honesta que já está
   implementada: **Origem desconhecida**.

### O que fazer em vez disso

- **Mostrar o estado do diário, já que consultar é grátis.** "O Windows registra
  mudanças de arquivo neste disco, guardando cerca de 32 MB de histórico" é
  informação verdadeira, barata e útil para quem quiser investigar por fora.

- **Manter a elevação como capacidade opcional, nunca exigência.** Se um dia
  entrar, entra como "observar com privilégios", ligada pelo usuário, com o
  programa funcionando igual sem ela.

- **Continuar melhorando os perfis por revisão.** Cada perfil escrito depois de
  olhar a pasta de verdade cobre mais casos do que o USN cobriria, e sem exigir
  nada do usuário.

### Quando reabrir esta decisão

Se aparecer um caso concreto em que o usuário precisa saber **qual processo**
escreveu e a resposta "Origem desconhecida" não basta. Aí a conversa é sobre
ETW com sessão elevada opcional — e a primeira coisa a fazer será medir o custo
que esta investigação deixou registrado como desconhecido.

---

## 4. O que entrou de código nesta etapa

Nada de USN ou ETW. O que a E9 entregou foi o **motor de comandos oficiais**:
as limpezas que a própria Microsoft mantém, acionadas por uma lista fechada em
tempo de compilação, com saída interpretada e registro no histórico.

Ver `src/commands/command_catalog.cpp` e `src/commands/command_runner.cpp`.

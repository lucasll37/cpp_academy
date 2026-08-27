# sfml_bullet2 — Juntas, Cordas e Veículos com Bullet3

## O que é este projeto?

Uma **sala 2D interativa** onde convivem cinco subsistemas do Bullet3 que o
[sfml_bullet](../sfml_bullet/README.md) não mostra: **juntas articuladas** (um boneco
ragdoll), **soft body** (uma corda que estica e balança), **veículo por raycast** (um carro
com suspensão), **explosão radial** e **ray picking** (pegar um objeto com o mouse e
arrastá-lo). O SFML só desenha; toda a simulação é do Bullet.

```bash
make configure            # só na primeira vez
source build/conanbuild.sh
meson compile -C build sfml_bullet2
./dist/bin/sfml_bullet2
```

O binário se chama exatamente `sfml_bullet2` — é o nome do alvo em
[`meson.build`](meson.build) e o arquivo instalado em `dist/bin/`.

**Controles reais**, lidos de `Simulation::handle_events()`:

| Entrada | Efeito |
|---|---|
| Botão **esquerdo** (segurar) | *ray picking*: agarra o corpo sob o cursor e arrasta |
| Botão **direito** | cria uma esfera no ponto clicado |
| Botão **do meio** | cria uma caixa no ponto clicado |
| **Roda do mouse** | explosão radial na posição do cursor |
| **← ↑ ↓ →** | dirige o veículo (↑ acelera, ↓ ré, ← → esterçam) |
| **E** | cria um ragdoll novo em (0 m, 4,5 m) |
| **B** | explosão na posição do cursor |
| **P** | pausa / retoma |
| **R** | reinicia a cena inteira |
| **Esc** | sai |

> ⚠ **As teclas `E`, `B` e as setas só respondem se existir um veículo na cena.** Em
> `handle_events()` elas estão dentro de `if (has_vehicle_)`. Como `R` recria o veículo,
> na prática funcionam sempre — mas é um acoplamento acidental, não um recurso.

> ⚠ Não existe tecla `C`. Se você leu isso em alguma versão antiga deste README, era falso.

---

## Antes de começar: os conceitos que você precisa

Esta seção não pressupõe nada além de C++ básico. Cada termo usado adiante é definido aqui.

### Corpo rígido (*rigid body*)

Um objeto que **não deforma**. Todo o seu estado cabe em poucos números: posição do centro
de massa, orientação, velocidade linear, velocidade angular. Se você sabe onde está o centro
e como o corpo está girado, sabe onde está cada ponto dele. É o modelo mais barato de
simular, e por isso é o padrão em jogos.

No Bullet, um corpo rígido é um `btRigidBody` e precisa de três coisas:

```
btCollisionShape  →  a FORMA (esfera, caixa, cápsula). Pode ser compartilhada.
btMotionState     →  onde ele ESTÁ (posição + orientação). Você lê daqui para desenhar.
massa             →  0 = estático (nunca se move); > 0 = dinâmico.
```

Massa zero não significa "leve": significa **infinitamente pesado**. Chão e paredes têm
massa 0.

### Passo de simulação (*step*)

A física não é contínua; ela avança em saltos. `stepSimulation(dt, maxSubSteps, fixedStep)`
recebe o tempo real decorrido e o divide internamente em pedaços de tamanho fixo:

```
dt real = 0,033 s          fixedStep = 1/60 = 0,0167 s
  |------------------------|
  [--sub 1--][--sub 2--]  resto guardado para o próximo frame
```

Passos fixos são o que torna a simulação **estável e reproduzível**. Aqui:
`world_->stepSimulation(dt, 10, 1.f / 60.f)` — até 10 sub-passos de 1/60 s por frame.

### Impulso (Ns) — e por que não é força

Uma **força** (N) atua ao longo do tempo. Um **impulso** (N·s) é aplicado de uma vez e muda
a velocidade instantaneamente:

```
Δv = impulso / massa
```

Um impulso de 25 N·s numa esfera de 1 kg dá 25 m/s. Na mesma esfera com 5 kg, dá 5 m/s. É
por isso que explosões usam impulso: o efeito é imediato, não precisa esperar frames.
`applyCentralImpulse(v)` empurra o centro de massa (só translação);
`applyTorqueImpulse(v)` faz girar.

### Restituição e atrito

- **Restituição** (`m_restitution`, 0…1) é o *quique*. 0 = massa de modelar, cai e para.
  1 = bola de borracha ideal, volta à altura original. Bullet combina os dois corpos em
  contato (por padrão, multiplicando).
- **Atrito** (`m_friction`) é a resistência ao deslizamento. 0 = gelo. 1 = borracha em
  asfalto.

### Amortecimento (`setDamping(linear, angular)`)

Dois números entre 0 e 1 que **drenam velocidade a cada passo**, independentemente de
colisão. `setDamping(0.8f, 0.8f)` é uma sopa grossa: o corpo perde 80% do excesso de
velocidade rapidamente e para de oscilar. `setDamping(0.05f, 0.05f)` é quase vácuo. O
projeto usa isso no ray picking: enquanto você segura um objeto, o damping sobe para 0,8 e
ele para de tremer; ao soltar, volta a 0,05.

### Grau de liberdade (DOF)

Um corpo rígido livre no espaço 3D tem **6 DOF**: pode transladar em X, Y, Z e girar em
torno de X, Y, Z. "Restringir um DOF" é proibir um desses movimentos.

Este projeto é 2D, e a maneira como ele consegue isso é travando DOF diretamente
(`PhysicsWorld::add_rigid`):

```cpp
rb->setLinearFactor (btVector3(1.f, 1.f, 0.f));  // pode mover em X e Y, nunca em Z
rb->setAngularFactor(btVector3(0.f, 0.f, 1.f));  // só gira em torno de Z
```

Cada componente é um **multiplicador** aplicado à velocidade daquele eixo. Zero = travado.
Restam 3 DOF: X, Y e a rotação no plano da tela.

### Constraint (vínculo)

Uma **constraint é uma equação que o solver força a valer entre dois corpos, a cada
passo**. "O ponto P do corpo A deve coincidir com o ponto Q do corpo B" é uma constraint.
O solver não move os corpos à mão: ele calcula os **impulsos corretivos** que fazem a
equação passar a valer, e os aplica junto com os impulsos de colisão.

Toda constraint **remove DOF**. É a forma mais direta de classificá-las:

| Classe do Bullet | O que trava | DOF rotacionais restantes | Analogia |
|---|---|---|---|
| `btPoint2PointConstraint` | as 3 translações relativas | 3 (gira livre em tudo) | rótula de esqueleto, corrente |
| `btHingeConstraint` | 3 translações + 2 rotações | 1 | dobradiça de porta, joelho |
| `btConeTwistConstraint` | as 3 translações, e **limita** as 3 rotações | 3, dentro de um cone + torção | ombro, quadril, pescoço |

O `btConeTwistConstraint` é o que este projeto usa. Seus três limites:

```
                  ▲ eixo do osso (twist)
     swingSpan1   │   swingSpan2 = abertura no OUTRO plano
    (abertura no  │
     plano 1)  ╲  │  ╱
                ╲ │ ╱
                 ╲│╱          o osso pode apontar para qualquer
    ══════════════●══════     direção DENTRO deste cone elíptico,
                 torso        e girar ±twistSpan em torno de si mesmo
```

- `swingSpan1` — meia-abertura do cone num plano (em radianos).
- `swingSpan2` — meia-abertura no plano perpendicular. Se for diferente de `swingSpan1`, o
  cone vira elíptico: um ombro abre mais para os lados do que para a frente.
- `twistSpan` — quanto o osso pode rodar em torno do próprio eixo. É o movimento de girar
  a mão sem mexer o cotovelo.

### Pivot e frame local

Cada corpo tem um **espaço local**: um sistema de coordenadas cuja origem é o centro de
massa do corpo e que gira junto com ele. `(0, 0.45, 0)` no espaço local do torso significa
"45 cm acima do centro do torso, sempre — mesmo se o torso estiver de cabeça para baixo".

Uma constraint amarra **um ponto local de A** a **um ponto local de B**. Chamamos esse
ponto de **pivot**. Quando a constraint também precisa de orientação (e não só de posição),
passa-se um `btTransform` inteiro em vez de um `btVector3`, e aí ele se chama **frame**:

```
     espaço local do TORSO              espaço local da CABEÇA
     ┌──────────────┐                   ┌────────┐
     │      ● (0, 0.45, 0)  ←── mesma ──→  ● (0, -0.18, 0)
     │      ▲       │       posição no    │        │
     │   centro     │        MUNDO        │ centro │
     └──────────────┘                   └────────┘
```

O solver empurra os dois corpos até que esses dois pontos ocupem o mesmo lugar no mundo.

### `disableCollisionBetweenLinkedBodies`

`world->addConstraint(c, true)` — o segundo argumento. Com `true`, os dois corpos ligados
pela constraint **param de colidir entre si**. Isso é quase sempre o que você quer:
cabeça e torso se sobrepõem no pescoço, e sem esse `true` o detector de colisão tenta
separá-los enquanto a constraint tenta juntá-los. O resultado é um tremor permanente.

### Soft body

O oposto do corpo rígido: uma **nuvem de nós** (cada um com massa, posição e velocidade
próprias) ligados por **links elásticos**. Não há "a posição do objeto" — há N posições.

```
corpo rígido:  1 posição + 1 orientação        →  forma imutável
soft body:     N posições                      →  a forma É o estado
      ●───●───●───●───●───●    ← 6 nós, 5 links; cada link resiste ao esticamento
```

Uma corda é o soft body mais simples: os nós em linha. Um pano seria uma grade; um cubo de
gelatina, um volume.

### Raycast (lançamento de raio)

Perguntar ao mundo: "se eu disparar um segmento de reta de A até B, o que ele acerta
primeiro?". `world->rayTest(from, to, callback)` responde. É a operação por trás de duas
coisas neste projeto: a suspensão do veículo e o *ray picking*.

### Broadphase e solver — as duas metades de um passo

```
stepSimulation
   │
   ├─ BROADPHASE   "quais pares de objetos ESTÃO PERTO?"  (btDbvtBroadphase)
   │                 usa uma árvore de caixas envolventes (AABB); descarta,
   │                 em O(n log n), os pares que nem chegam perto de colidir
   │
   ├─ NARROWPHASE  "esses pares realmente se tocam? onde?" (btCollisionDispatcher)
   │
   └─ SOLVER       "que impulsos fazem os contatos e as constraints valerem?"
                     (btSequentialImpulseConstraintSolver — resolve um vínculo
                      de cada vez, várias passadas, até convergir)
```

### Ragdoll

Um **boneco articulado**: vários corpos rígidos (os ossos) unidos por constraints (as
juntas). Jogos usam ragdolls no instante em que um personagem perde o controle — a
animação pré-gravada é desligada e a física assume. O corpo cai de um jeito diferente a
cada vez, reagindo ao terreno e ao impacto, sem que ninguém tenha animado aquela queda.

---

## Por que estudar isto?

Porque tudo neste projeto existe para resolver um problema que "só corpos rígidos caindo"
não resolve:

| Sem isto | Com isto |
|---|---|
| Objetos caem, quicam e param. Nada é **articulado**. | Constraints unem corpos: bonecos, portas, correntes, guindastes. |
| Uma corda vira uma fileira de caixas que se atravessam. | Soft body: 22 nós, 41 links, esticando de verdade. |
| Um carro precisa de rodas simuladas como corpos rígidos com atrito — instável, lento, capota sozinho. | `btRaycastVehicle`: as rodas nem existem como corpos; são 4 raios e 4 molas. Estável e barato. |
| A única interação possível é criar objetos. | Ray picking: o mouse vira uma mão que pega, arrasta e solta. |
| Explosões viram "teleportar tudo para longe". | Impulso radial com atenuação por distância: quem está perto voa, quem está longe balança. |

E porque a alternativa a entender constraints é a superstição: "junta é só grudar dois
objetos" (não — é uma equação, e ela pode não convergir), "quanto mais rígida melhor"
(não — rigidez alta com passo grande explode a simulação), "ragdoll é um recurso do
motor" (não — é uma receita de 6 corpos e 5 constraints que você monta à mão, como aqui).

---

## Estrutura de arquivos

```
src/sfml_bullet2/
├── main.cpp              ← 46 linhas: só cria Simulation e chama run().
│                            O cabeçalho é o índice dos recursos demonstrados.
├── physics_world.hpp     ← o mundo: por que btSoftRigidDynamicsWorld e não
├── physics_world.cpp        btDiscreteDynamicsWorld; add_rigid/add_soft;
│                            posse (ownership) de shapes, bodies e motion states;
│                            é aqui que a restrição 2D (setLinearFactor) é aplicada
├── ragdoll.hpp           ← 6 segmentos, 5 btConeTwistConstraint com limites.
├── ragdoll.cpp              Ensina: frames locais, swing/twist, apply_impulse
├── rope.hpp              ← btSoftBody via btSoftBodyHelpers::CreateRope.
├── rope.cpp                 Ensina: kLST/kAST/kDP, âncora fixa, appendAnchor
├── simulation.cpp        ← 756 linhas, o arquivo grande. Contém, nesta ordem:
│                            build_scene, build_vehicle (btRaycastVehicle),
│                            spawn_*, explode, ray picking em handle_events,
│                            update, sync_entities, sync_vehicle,
│                            draw_soft_bodies, draw_ragdolls, draw_hud, render, run
├── simulation.hpp        ← SCALE/ORIGIN e as conversões metro↔pixel;
│                            struct Entity (par corpo↔forma) e VehicleEntity
├── meson.build           ← executable('sfml_bullet2', ...)
└── README.md
```

---

## O mundo: por que `btSoftRigidDynamicsWorld`

O tipo do mundo decide **quais tipos de objeto podem coexistir**. A escolha real está na
*collision configuration*, que é o catálogo de algoritmos de colisão registrados:

| Configuração | Pares que sabe tratar |
|---|---|
| `btDefaultCollisionConfiguration` | rígido × rígido |
| `btSoftBodyRigidBodyCollisionConfiguration` | rígido × rígido, **soft × rígido, soft × soft** |

Como este projeto tem uma corda, o mundo precisa ser `btSoftRigidDynamicsWorld` e a
configuração precisa ser a segunda. Trocar por `btDiscreteDynamicsWorld` compila, mas a
corda atravessa tudo.

Há um segundo objeto que só existe por causa do soft body: o `btSoftBodyWorldInfo`. É um
**bloco de parâmetros compartilhado** que todo `btSoftBody` consulta durante o solve —
gravidade, densidade do ar e da água, e ponteiros para o dispatcher e o broadphase. Ele
não é preenchido sozinho:

```cpp
// physics_world.cpp
soft_info_.m_broadphase = broadphase_.get();
soft_info_.m_dispatcher = dispatcher_.get();
soft_info_.m_gravity    = btVector3(0.f, -9.81f, 0.f);
soft_info_.m_sparsesdf.Initialize();   // grade de distância usada nas colisões soft
```

> ⚠ Esquecer `m_sparsesdf.Initialize()` ou os dois ponteiros é a causa nº 1 de
> *segmentation fault* no primeiro `stepSimulation` com soft bodies. O erro não aparece na
> criação da corda; aparece no passo seguinte.

### Quem é dono do quê

O Bullet **não** libera nada que você criar. `PhysicsWorld` resolve isso guardando
`unique_ptr` em três vetores e expondo `take_shape` / `take_body` / `take_state`. O padrão
de uso é sempre o mesmo:

```cpp
auto s = std::make_unique<btBoxShape>(btVector3(0.25f, 0.25f, 0.25f));
btBoxShape* sh = s.get();          // guarda o ponteiro cru para usar
pw.take_shape(std::move(s));       // entrega a posse ao PhysicsWorld
pw.add_rigid(sh, 1.2f, transform, 0.2f, 0.6f);
```

> ⚠ **Constraints vazam neste projeto.** O destrutor de `PhysicsWorld` faz
> `world_->removeConstraint(...)` mas nunca `delete`. As 5 constraints de cada ragdoll são
> criadas com `new` e nunca liberadas. É pouca memória e o processo morre logo depois —
> mas se você copiar este código para algo de longa duração, guarde as constraints em
> `unique_ptr` também. A constraint do ray picking, essa sim, é deletada corretamente.

---

## Demo 1 — Constraints, do mínimo ao ragdoll

### O exemplo mínimo: um pêndulo (2 corpos, 1 constraint)

Antes de olhar o ragdoll, monte isto mentalmente. É o menor programa possível que usa uma
junta, e usa apenas a API real deste projeto:

```cpp
// Corpo A: âncora fixa no teto. Massa 0 → nunca se move.
auto* teto = pw.add_rigid(new_sphere(0.05f, pw), 0.f, make_t(0.f, 5.f));

// Corpo B: o peso do pêndulo, 1 kg, 1 metro abaixo.
auto* peso = pw.add_rigid(new_sphere(0.20f, pw), 1.f, make_t(0.f, 4.f));

// A constraint: "o ponto (0,0,0) local do teto deve coincidir
//                com o ponto (0,+1,0) local do peso".
auto* c = new btPoint2PointConstraint(
    *teto, *peso,
    btVector3(0.f,  0.f, 0.f),   // pivot A, no espaço local do teto
    btVector3(0.f,  1.f, 0.f));  // pivot B, no espaço local do peso

pw.world()->addConstraint(c, /*disableCollisionBetweenLinkedBodies=*/true);
```

Solte, e o peso descreve um arco. As 3 translações relativas foram travadas; as rotações
não. É um pêndulo.

Troque `true` por `false` no `addConstraint` e o pêndulo passa a tremer: as duas esferas
se tocam no ponto de junção, a colisão empurra, a constraint puxa de volta, e o solver
oscila entre as duas a 60 Hz.

### O ragdoll de verdade (6 corpos, 5 constraints)

`Ragdoll::build()` monta o boneco em duas fases: primeiro os corpos, depois as juntas.

```cpp
enum Segment { TORSO=0, HEAD, ARM_L, ARM_R, LEG_L, LEG_R, COUNT };  // COUNT == 6
```

| Segmento | Forma | Dimensões (m) | Massa (kg) |
|---|---|---|---|
| `TORSO` | `btCapsuleShape` | raio 0,20 · altura 0,60 | 5,0 |
| `HEAD` | `btSphereShape` | raio 0,18 | 1,5 |
| `ARM_L` / `ARM_R` | `btCapsuleShape` | raio 0,08 · altura 0,35 | 0,8 |
| `LEG_L` / `LEG_R` | `btCapsuleShape` | raio 0,09 · altura 0,45 | 1,5 |

São **6** segmentos, não 7. Não há antebraço nem canela — logo, **não há nenhum
`btHingeConstraint` neste ragdoll**: cotovelo e joelho precisariam de segmentos que este
modelo não tem. As 5 juntas são todas `btConeTwistConstraint`.

A cápsula é a forma preferida para membros: é um cilindro com semiesferas nas pontas, não
tem quinas para prender em outras formas, e o solver lida melhor com ela do que com uma
caixa girando dentro de uma junta.

O pescoço, literal de `ragdoll.cpp`:

```cpp
btTransform frame_torso; frame_torso.setIdentity();
frame_torso.setOrigin(btVector3(0.f, 0.45f, 0.f));   // topo do torso, espaço local

btTransform frame_head;  frame_head.setIdentity();
frame_head.setOrigin(btVector3(0.f, -0.18f, 0.f));   // base da cabeça, espaço local

auto* c = new btConeTwistConstraint(*bodies[TORSO], *bodies[HEAD],
                                    frame_torso, frame_head);
c->setLimit(M_PI * 0.15f,   // swingSpan1 — 27°
            M_PI * 0.15f,   // swingSpan2 — 27°
            M_PI * 0.10f);  // twistSpan  — 18°
c->setDamping(0.3f);
world->addConstraint(c, true);
```

Os limites de todas as 5 juntas, convertidos para graus:

| Junta | swingSpan1 | swingSpan2 | twistSpan | Leitura |
|---|---|---|---|---|
| Pescoço | 27° | 27° | 18° | mal consegue inclinar; nunca gira 180° |
| Ombro E / D | 90° | 90° | 72° | braço percorre um hemisfério inteiro |
| Quadril E / D | 54° | 54° | 18° | perna abre bem menos que o braço |

> ⚠ **Sem `setLimit`, o ragdoll vira espaguete.** O `btConeTwistConstraint` sem limites é
> uma rótula livre: a cabeça pode dar voltas completas, o braço pode dobrar para dentro do
> torso, e o boneco converge para uma pose anatomicamente impossível — que é exatamente o
> visual de "bug de física" que se vê em jogos mal ajustados. Os limites não são
> refinamento; são o que faz o boneco parecer um corpo.

> ⚠ **`setCenterOfMassTransform` depois de `add_rigid`.** Os braços são criados na
> vertical (a cápsula do Bullet nasce alinhada com Y) e depois rotacionados 90° em torno de
> Z com `bodies[ARM_L]->setCenterOfMassTransform(t)`. Funciona aqui porque acontece antes
> do primeiro `stepSimulation`, mas é frágil: teleportar um corpo já registrado no mundo
> não atualiza sozinho o AABB dele no broadphase nem o `btMotionState`, e um corpo
> adormecido nem sequer nota a mudança. O caminho robusto é passar a rotação já pronta no
> `btTransform` de criação.

> ⚠ **Nunca chame `setMassProps(0, ...)` depois de `addRigidBody`** para "congelar" um
> corpo. Ele já foi classificado como dinâmico quando entrou no mundo, e o broadphase
> continua tratando-o assim. O certo é criar com massa 0 desde o início, ou remover do
> mundo, alterar e re-adicionar.

### `apply_impulse` — como o ragdoll reage a uma explosão

```cpp
void Ragdoll::apply_impulse(const btVector3& impulse, const btVector3& origin) {
    for (int i = 0; i < COUNT; ++i) {
        if (!bodies[i]) continue;
        bodies[i]->activate(true);                 // acorda o corpo

        btVector3 rel = bodies[i]->getCenterOfMassPosition() - origin;
        float dist = rel.length();
        if (dist < 0.001f) dist = 0.001f;          // guarda contra divisão por zero

        float scale = 1.f / (1.f + dist * dist * 0.5f);   // falloff quadrático
        bodies[i]->applyCentralImpulse(impulse * scale);
    }
}
```

Duas leituras honestas do que este código faz:

1. `activate(true)` é obrigatório. O Bullet **adormece** corpos parados (*sleeping*) para
   economizar CPU: eles saem do solver e param de responder a forças. Sem acordar, o
   impulso é ignorado silenciosamente.
2. **O impulso aqui não é radial.** O parâmetro `impulse` é usado com a direção que veio de
   fora, e só a *intensidade* é atenuada pela distância. `Simulation::explode()` chama isto
   com `btVector3(0, 12.5f, 0)` — ou seja, os segmentos do ragdoll levam um empurrão **para
   cima**, mais fraco quanto mais longe do estouro. Os corpos rígidos comuns, esses sim,
   recebem impulso radial (veja o Demo 4). É uma inconsistência do projeto, não uma regra
   do Bullet.

> ⚠ **NaN é contagioso em física.** Se `dist` fosse 0 e o código chamasse `rel.normalized()`,
> o resultado seria `NaN`. Um único NaN entra no solver, contamina as velocidades acopladas
> por contatos e constraints, e em poucos frames a cena inteira desaparece — os corpos vão
> para "coordenada nenhuma". Por isso a linha `if (dist < 0.001f) dist = 0.001f;` existe.
> Toda normalização em código de física precisa de uma guarda dessas.

---

## Demo 2 — A corda: um `btSoftBody` de verdade

A corda **não** é uma fileira de cápsulas unidas por `btPoint2PointConstraint`. É um
`btSoftBody`, criado por um helper do Bullet:

```cpp
// rope.cpp — literal
body = btSoftBodyHelpers::CreateRope(
    pw.soft_info(),                 // o bloco de parâmetros compartilhado
    btVector3(x0, y0, 0.f),         // ponto inicial
    btVector3(x1, y1, 0.f),         // ponto final
    segments,                       // subdivisões
    1);                             // bitmask de nós fixos: 1 = ancora o nó 0
```

O bitmask `fixeds`: `1` fixa o **primeiro** nó, `2` fixa o **último**, `3` fixa os dois.
"Fixar" é literalmente `setMass(node, 0)` — um nó de massa zero não é movido por nada.

**Medido**: a simulação cria a corda com `segments = 20`. O helper gera `res + 2` nós, e
`generateBendingConstraints(2)` acrescenta uma segunda camada de links. Instrumentando o
construtor e rodando o binário:

```
nos_corda=22   links_corda=41
```

22 nós, 41 links: 21 links da cadeia (nó *i* ↔ nó *i*+1) mais 20 links de **flexão** (nó
*i* ↔ nó *i*+2). Esses últimos são o que impede a corda de dobrar em ângulo reto —
sem eles, a cadeia é infinitamente flexível.

### Os parâmetros de material, um por um

```cpp
btSoftBody::Material* mat = body->m_materials[0];
mat->m_kLST = 0.8f;    // Linear STiffness  — resistência ao ALONGAMENTO   [0..1]
mat->m_kAST = 0.5f;    // Angular STiffness — resistência à CURVATURA      [0..1]

body->m_cfg.kDP  = 0.05f;  // Damping — drena velocidade dos nós; 0 = corda que nunca para
body->m_cfg.kDG  = 0.01f;  // Drag    — arrasto do ar
body->m_cfg.kSHR = 1.0f;   // Shear   — resistência ao cisalhamento
body->m_cfg.kCHR = 1.0f;   // Contact Hardness contra corpos RÍGIDOS; 1 = não afunda

body->setTotalMass(1.0f, true);   // 1 kg distribuído pelos nós; true = por volume
body->generateBendingConstraints(2);
```

Intuição de cada faixa:

| Parâmetro | Perto de 0 | Perto de 1 |
|---|---|---|
| `m_kLST` | elástico de cueca: estica muito | corrente de aço: comprimento fixo |
| `m_kAST` | cadeia mole, dobra em qualquer ângulo | mangueira grossa, resiste a dobrar |
| `kDP` | balança para sempre | para quase instantaneamente |
| `kCHR` | a corda afunda dentro das caixas | a corda repousa sobre elas |

> ⚠ `m_kLST` alto (perto de 1) com poucos sub-passos **explode**. Rigidez é resolvida por
> iteração; se o solver não tem passos suficientes para convergir, ele responde com
> impulsos crescentes. Se a corda começar a vibrar e crescer, baixe `kLST` antes de aumentar
> a rigidez.

### Ancorar a ponta num corpo rígido

`fixeds` prende o nó a uma **posição no mundo**. Para prender a um corpo que se move, o
mecanismo é outro:

```cpp
if (attach) {
    int last = body->m_nodes.size() - 1;
    body->appendAnchor(last, attach, /*disableCollision=*/false, /*influence=*/1.0f);
}
```

`influence = 0` → a corda segue o corpo mas não o puxa. `influence = 1` → a corda também
arrasta o corpo. Este projeto passa 1,0, mas a cena inicial chama
`rope.build(physics_, 2.f, 5.5f, 2.f, 2.0f, 20)` **sem** o parâmetro `attach` — ou seja, a
corda pendurada do teto tem a ponta de baixo solta.

A assinatura real, para não errar a ordem dos argumentos:

```cpp
void Rope::build(PhysicsWorld& pw,
                 float x0, float y0,    // ponto de cima (ancorado)
                 float x1, float y1,    // ponto de baixo
                 int segments,
                 btRigidBody* attach = nullptr);
```

Desenhar um soft body é diferente de desenhar um corpo rígido: não há uma transformação
para ler, há N nós. `draw_soft_bodies()` percorre `rope.body->m_nodes` e monta um
`sf::VertexArray(sf::LineStrip)` com um ponto por nó, mais um círculo em cada nó.

---

## Demo 3 — O veículo: suspensão sem simular a suspensão

Um carro simulado "direito" seria um chassi mais quatro rodas rígidas, cada uma presa por
uma junta com mola. Isso é caro, instável e capota sozinho. O `btRaycastVehicle` faz outra
coisa: **as rodas não existem como corpos**.

```
        ┌──────── chassi (o ÚNICO btRigidBody do veículo) ────────┐
        │                                                        │
        ● ponto de conexão                     ponto de conexão ●
        ┊                                                        ┊
        ┊ raio disparado para baixo (wheel_dir = 0,-1,0)          ┊
        ┊ comprimento = suspensionRestLength + raio da roda       ┊
        ▼                                                        ▼
    ────●────────────────────────────────────────────────────────●──── solo
        ↑ o que o raio acerta define quanto a mola COMPRIMIU,
          e a mola devolve uma força para cima no chassi
```

Cada frame, para cada roda: dispara um raio, mede a distância até o solo, calcula a
compressão da mola e aplica a força resultante no chassi. Só isso. Estável, barato, e é o
que praticamente todo jogo de corrida arcade faz.

Montagem, de `build_vehicle()`:

```cpp
vehicle_.chassis = physics_.add_rigid(cs, 3.0f, make_t(cx, cy), 0.1f, 0.5f);
vehicle_.chassis->setAngularFactor(btVector3(0.f, 0.f, 1.f));

vehicle_.tuning    = std::make_unique<btRaycastVehicle::btVehicleTuning>();
vehicle_.raycaster = std::make_unique<btDefaultVehicleRaycaster>(physics_.world());
vehicle_.vehicle   = std::make_unique<btRaycastVehicle>(
    *vehicle_.tuning, vehicle_.chassis, vehicle_.raycaster.get());

physics_.world()->addAction(vehicle_.vehicle.get());   // o mundo chama updateVehicle()
vehicle_.vehicle->setCoordinateSystem(0, 1, 2);        // right=X, up=Y, forward=Z
```

`addAction` é o que faz o veículo funcionar: uma *action* é um objeto que o mundo chama a
cada sub-passo, antes do solver. Sem essa linha, os raios nunca são disparados.

`addWheel(connectionPoint, wheelDirection, wheelAxle, suspensionRestLength, wheelRadius,
tuning, isFrontWheel)`. Os valores reais:

| Parâmetro | Valor | O que significa |
|---|---|---|
| `wheelDirection` | `(0, -1, 0)` | a suspensão comprime para baixo |
| `wheelAxle` | `(0, 0, 1)` | eixo de rotação da roda — Z, o único eixo livre em 2D |
| `suspensionRestLength` | 0,30 m | comprimento da mola sem carga |
| `wheelRadius` | 0,22 m | |
| `m_suspensionStiffness` | 20 | rigidez da mola. Alto = duro, esportivo; baixo = flutuante |
| `m_wheelsDampingRelaxation` | 2,3 | amortecimento quando a mola ESTENDE |
| `m_wheelsDampingCompression` | 4,4 | amortecimento quando a mola COMPRIME |
| `m_frictionSlip` | 1000 | aderência do pneu. Baixo (~1) = derrapa; 1000 = colado |
| `m_rollInfluence` | 0,1 | quanto a força lateral tenta capotar o carro. 0 = nunca capota |

Comprimir e estender têm amortecimentos **diferentes** de propósito: um carro real absorve
o buraco rápido (compressão amortecida forte) e devolve devagar (relaxamento mais suave).

Tração e direção são aplicadas **por índice de roda**, em `update()`:

```cpp
v.applyEngineForce(engine_force_, 2);   // rodas traseiras
v.applyEngineForce(engine_force_, 3);
v.setSteeringValue(steering_, 0);       // rodas dianteiras
v.setSteeringValue(steering_, 1);
float brake = (engine_force_ == 0.f) ? 0.5f : 0.f;   // freio quando solta o acelerador
v.setBrake(brake, 0);
v.setBrake(brake, 1);
```

> ⚠ **A seta ↑ aplica força negativa** (`engine_force_ = -15.f`) e a ↓ aplica positiva.
> Não é erro: o eixo *forward* configurado em `setCoordinateSystem(0,1,2)` é o Z, e o carro
> é desenhado de perfil no plano XY. O sinal foi escolhido para bater com a orientação da
> tela. Se você mexer em `setCoordinateSystem`, revise esses sinais.

> ⚠ **As quatro rodas ocupam só dois pontos.** Os pontos de conexão declarados são
> `(-0.55, -0.05, 0)`, `(0.55, -0.05, 0)`, `(-0.55, -0.05, 0)`, `(0.55, -0.05, 0)` — a
> dianteira esquerda e a traseira esquerda estão **exatamente no mesmo lugar**. Faz sentido
> num mundo restrito ao plano XY (não há profundidade para separar dianteira de traseira em
> Z), mas significa que "4 rodas" aqui são dois pares sobrepostos: um par esterça, o outro
> traciona.

---

## Demo 4 — Explosão radial com atenuação

```cpp
const float RADIUS   = 4.f;    // metros
const float STRENGTH = 25.f;

for (cada btCollisionObject do mundo) {
    btRigidBody* rb = btRigidBody::upcast(obj);
    if (!rb || rb->getMass() == 0.f) continue;      // estáticos não voam

    btVector3 rel  = rb->getCenterOfMassPosition() - origin;
    float     dist = rel.length();
    if (dist > RADIUS) continue;                     // fora do alcance

    float scale = STRENGTH * (1.f - dist / RADIUS) / std::max(dist, 0.1f);
    rb->activate(true);
    rb->applyCentralImpulse(rel.normalized() * scale);      // empurra para FORA
    rb->applyTorqueImpulse(btVector3(0.f, 0.f, scale * 0.3f)); // e faz girar
}
```

Três decisões que valem a pena entender:

1. **A atenuação é linear** — `(1 - dist/RADIUS)` vale 1 no epicentro e 0 na borda. Uma
   explosão física real cai com o quadrado da distância; a linear é mais previsível para
   jogos, porque garante zero exato no raio declarado.
2. **A divisão por `dist`** existe porque `rel` ainda não foi normalizado no cálculo do
   `scale`; o `std::max(dist, 0.1f)` evita a divisão por zero.
3. **`applyTorqueImpulse` só no eixo Z** — é o único eixo de rotação livre neste mundo 2D.

> ⚠ `rel.normalized()` **não** está protegido. Se um corpo estivesse exatamente sobre
> `origin`, `dist` seria 0, o teste `dist > RADIUS` deixaria passar, e `normalized()`
> devolveria NaN. É o mesmo perigo do `apply_impulse` do ragdoll — só que ali há guarda e
> aqui não. Ao copiar este código, acrescente `if (dist < 1e-4f) continue;`.

---

## Demo 5 — Ray picking: transformar o mouse numa mão

Esta é a parte mais instrutiva do arquivo. São quatro etapas.

**1. Pixel → raio.** Como o mundo é 2D no plano XY, o raio é simplesmente uma linha
perpendicular à tela, atravessando o plano:

```cpp
static btVector3 screen_to_ray_from(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);       // pixel → metro
    return btVector3(bv.x, bv.y, 1.f);   // 1 m na FRENTE do plano
}
static btVector3 screen_to_ray_to(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    return btVector3(bv.x, bv.y, -1.f);  // 1 m ATRÁS
}
```

**2. Perguntar ao mundo o que o raio acerta.**

```cpp
btCollisionWorld::ClosestRayResultCallback cb(from, to);
physics_.world()->rayTest(from, to, cb);
if (cb.hasHit()) { /* cb.m_collisionObject, cb.m_hitPointWorld */ }
```

`ClosestRayResultCallback` guarda apenas o **primeiro** acerto. Existe também
`AllHitsRayResultCallback`, que guarda todos.

**3. Converter o ponto de impacto para o espaço local do corpo.** O raio devolve uma
posição no **mundo**; a constraint precisa de um pivot no espaço **local** do corpo, para
que o ponto agarrado continue sendo o mesmo enquanto o objeto gira:

```cpp
btVector3 local = rb->getCenterOfMassTransform().inverse() * cb.m_hitPointWorld;
auto* c = new btPoint2PointConstraint(*rb, local);   // constraint de UM corpo só
c->m_setting.m_impulseClamp = 30.f;
c->m_setting.m_tau          = 0.001f;
physics_.world()->addConstraint(c);
```

Um `btPoint2PointConstraint` com **um** corpo amarra o pivot local a um ponto **fixo no
mundo** — e esse ponto é o que atualizamos a cada movimento do mouse.

- `m_impulseClamp = 30` limita o impulso máximo que a constraint pode aplicar por passo.
  Sem esse teto, arrastar rápido catapulta o objeto (e pode desestabilizar tudo que
  encostar nele).
- `m_tau = 0.001` é o fator de correção de erro posicional. O padrão do Bullet é 0,3. Um
  tau **menor** significa correção **mais fraca e mais lenta** — a pegada fica mole,
  elástica, e o objeto arrasta atrás do cursor em vez de grudar nele. (O comentário no
  código chama isso de "resposta imediata"; é o contrário. Aumente para ~0,3 se quiser
  uma pegada firme.)

**4. Arrastar e soltar.**

```cpp
// a cada sf::Event::MouseMoved:
pick_constraint_->setPivotB(btVector3(bv.x, bv.y, 0.f));

// ao soltar o botão:
physics_.world()->removeConstraint(pick_constraint_);
delete pick_constraint_;
picked_body_->setDamping(0.05f, 0.05f);   // devolve o damping normal
```

Ao agarrar, o código sobe o damping para `(0.8, 0.8)`; ao soltar, devolve para
`(0.05, 0.05)`. Sem isso, o objeto oscila em torno do cursor como um peso na ponta de uma
mola.

---

## Demo 6 — Sincronizar Bullet → SFML

O Bullet trabalha em **metros**, com Y para cima e origem no centro do mundo. O SFML
trabalha em **pixels**, com Y para baixo e origem no canto superior esquerdo. A ponte está
em `simulation.hpp`:

```cpp
constexpr float SCALE    = 80.f;    // 1 metro = 80 pixels
constexpr float ORIGIN_X = 400.f;   // onde fica x=0 na tela (janela de 800×600)
constexpr float ORIGIN_Y = 540.f;   // onde fica y=0

inline sf::Vector2f to_sfml(float bx, float by) {
    return { bx * SCALE + ORIGIN_X, -by * SCALE + ORIGIN_Y };   // note o menos
}
```

E a rotação, em `sync_entities()`:

```cpp
btScalar  angle = t.getRotation().getAngle();
btVector3 axis  = t.getRotation().getAxis();
float deg = angle * (180.f / 3.14159f);
if (axis.getZ() < 0.f) deg = -deg;      // ⚠ o passo que quase todo mundo esquece
e.visual->setRotation(-deg);
```

> ⚠ **`getAngle()` sozinho está errado.** Um quatérnio devolve sempre um ângulo positivo
> (0…π) em torno de um eixo que pode apontar para +Z **ou** para −Z. Ignorar `getAxis()`
> faz o objeto girar sempre no mesmo sentido, "pulando" quando o eixo se inverte. Aqui o
> mundo é 2D e só há um eixo possível, então basta checar o sinal de Z. Em 3D, não existe
> um ângulo único — a rotação precisa ser aplicada como matriz ou quatérnio.

A cena inicial, **medida** instrumentando o construtor:

```
colObjs=19   rigid_static=5   rigid_dyn=13   soft=1   constraints=5   entities=6
```

- 5 rígidos estáticos: chão, parede esquerda, parede direita, duas plataformas inclinadas.
- 13 rígidos dinâmicos: 6 caixas da torre + 1 chassi do veículo + 6 segmentos do ragdoll.
- 1 soft body: a corda.
- 5 constraints: as 5 juntas cone-twist do ragdoll.
- 6 `Entity`: apenas as 6 caixas da torre têm par corpo↔forma no vetor `entities_`;
  ragdoll, corda e veículo são desenhados por funções próprias.

---

## Como validar

```bash
source build/conanbuild.sh
meson compile -C build sfml_bullet2
./dist/bin/sfml_bullet2
```

**Saída no terminal** (verificada):

```
Veículo criado em (-1.5, 0.6)
=== Bullet3 Física Avançada ===
  Clique esq        : arrastar objetos
  Clique dir        : spawn esfera
  Clique meio       : spawn caixa
  Scroll do mouse   : EXPLOSÃO
  Setas             : controle do veículo
  E                 : spawnar ragdoll
  B                 : explosão no cursor
  P                 : pausar/retomar
  R                 : resetar cena
  Esc               : sair
```

**Na janela (800×600)**: fundo escuro, chão e paredes cinzentas, um HUD preto no canto
superior esquerdo com os controles, contadores no canto superior direito
(`Esferas / Caixas / Ragdolls / Cordas`), uma torre de 6 caixas coloridas à esquerda, duas
plataformas inclinadas, um carro vermelho com rodas amarelas, um ragdoll marrom ao centro e
uma corda amarela pendurada do alto à direita.

Checklist do que deve acontecer:

| Ação | Resultado esperado | Se falhar |
|---|---|---|
| esperar 2 s | a torre de caixas assenta e para de tremer | passo de simulação instável |
| arrastar a corda com o botão esquerdo | ela **estica um pouco** e volta; o topo nunca se solta | `kLST` errado, ou o nó 0 não foi ancorado |
| segurar ← ou → | o carro esterça e anda sem capotar | `rollInfluence` alto demais |
| roda do mouse sobre a torre | as caixas voam para fora do ponto do cursor, girando | explosão sem `activate(true)` |
| **B** com o cursor sobre o ragdoll | ele é lançado para cima (não radialmente — veja o Demo 1) | |
| olhar o ragdoll depois de cair | nenhum membro atravessa o torso, nenhuma articulação inverte | `setLimit` ausente |
| **P** | aparece "PAUSADO" e tudo congela | |
| **R** | a cena volta ao estado inicial; contadores zeram | |

---

## Referências

Subprojetos irmãos deste repositório:

- [sfml_bullet](../sfml_bullet/README.md) — **pré-requisito**: o mesmo laço SFML+Bullet
  sem juntas, sem soft body e sem veículo. Comece por ele.
- [qt_sfml2](../qt_sfml2/README.md) — o mesmo Bullet, mas em 3D, dentro de uma janela Qt,
  com projeção perspectiva escrita à mão.
- [collision](../collision/README.md) — detecção de colisão sem motor nenhum: AABB, SAT,
  o que o broadphase faz por baixo.
- [flight-simulator](../flight-simulator/README.md) — integração numérica e passo fixo
  aplicados a um modelo de voo.
- [smart-pointers](../smart-pointers/README.md) — o `unique_ptr` que `PhysicsWorld` usa
  para segurar shapes, bodies e motion states.

Fontes externas canônicas:

- **Bullet Physics User Manual** (PDF no repositório do Bullet, `docs/`) — capítulo
  *Constraints* para `btConeTwistConstraint`/`btHingeConstraint`, e *Soft Body* para
  `kLST`/`kAST`/`kDP`.
- Código de referência do próprio Bullet: `examples/Constraints/`,
  `examples/SoftDemo/`, `examples/ForkLift/` (o exemplo canônico de `btRaycastVehicle`).
- <https://pybullet.org/Bullet/BulletFull/> — documentação Doxygen da API.
- Erin Catto, *Iterative Dynamics with Temporal Coherence* — o artigo que descreve o
  método de impulso sequencial que o `btSequentialImpulseConstraintSolver` implementa.
- <https://www.sfml-dev.org/documentation/2.6.1/> — `sf::VertexArray`, `sf::Shape`,
  `sf::Event`.

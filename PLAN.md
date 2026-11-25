# Horizon

O Horizon é o EglantinePS5 (`heydemoura/EglantinePS5`) com uma camada nossa por cima. O núcleo segue o upstream; a camada traz o que o diferencia:

- visual próprio do launcher;
- tradução PT-BR integrada (CNX);
- perfis separados para Switch 1 e Switch 2;
- patches de núcleo opcionais, ligados por jogo, para os títulos mais pesados.

## Repositório

| O quê | Onde |
|---|---|
| Branch de trabalho | `horizon` (local, em `~/Developer/Horizon/horizon-nx2`) |
| GitHub | `joaoalvess/horizon-ns2`, branch `main` (remote `github`) |
| Upstream | `heydemoura/EglantinePS5` (remote `upstream`) |
| Base atual | `v1.000.004` (`46e4b642`) |
| Referências | branches `rewrite/ps5-platform` (candidatos a patch) e `feat/launcher-from-switch`; repo `horizon-switch` |

## Regras da camada

1. Código nosso fica em arquivos novos. Cada alteração em arquivo do upstream é pequena e entra na tabela "Pontos de contato".
2. O núcleo (`Eglantine/`, `src/`) só muda por patch Horizon: compilado no build, desligado por padrão e ligado por jogo no launcher. Correção de bug que não é opcional vai como PR para o upstream.
3. Núcleo novo entra por merge da tag do upstream, nunca por rebase: o `main` do GitHub nunca é reescrito.
4. Versão: `contentVersion` `01.<patch do upstream>.<revisão nossa>` em `sce_sys/param.json` (base `v1.000.004` → `01.004.001`), tag `v1.004.001`. A revisão sobe a cada release nosso e volta a `001` quando a base muda. A base fica também em `UPSTREAM_CHECK_BASE_TAG` (`headless/update_check/upstream_check.h`).

## Release

- O `make test` do upstream (`.github/workflows/horizon-test.yml`) só roda à mão (Actions > Host tests), porque não compila no upstream. No `v1.000.004` ele não compila: `headless/memory_check.cpp:25` inclui `core/memory/read_traps.h` e `headless/nup_check.cpp` inclui `core/file_sys/nup.h`, que o núcleo não tem mais.
- Push de tag `v*`: o `release.yml` do upstream, sem mudança, builda o pacote (`make release`, que gera e confere o lapy e roda o `check_package.py`) e publica o pre-release se o build passar. Os nossos testes entram nesse gate a partir da etapa 2.
- As notas do release vêm da seção `## Changes in v<versão>` do `README.md` (`tools/ci/make-dist.py`). Sem ela, o `gh release create` falha: crie antes o release como rascunho com as notas (`gh release create <tag> --draft --notes-file ...`) e publique depois que o ZIP entrar.
- Release publicado: o `.github/workflows/horizon-update.yml` baixa o ZIP, gera `api/v1/apps/PPSA98515.json` e `api/v1/manifest.json` (sequence = segundos unix), assina com o secret `HORIZON_UPDATE_KEY` (`tools/horizon/sign-catalog.py`, que confere a assinatura contra as chaves de `self_update.c`) e publica no `gh-pages`. O app consulta `https://joaoalvess.github.io/horizon-ns2/api/v1/` e, se falhar, `https://raw.githubusercontent.com/joaoalvess/horizon-ns2/gh-pages/api/v1/`. Roda também depois de cada "Release build" de tag e à mão (`gh workflow run horizon-update.yml -f tag=<tag>`).
- Chaves Ed25519: `~/.config/horizon/update-signing.pem` (a do secret) e `~/.config/horizon/update-signing-spare.pem` (reserva). Nunca entram no git.
- Tempo: o primeiro build nosso, sem cache, levou 50 min; os de tag do upstream levaram de 40 a 66 min.

```fish
git push github horizon:main
git tag -a v1.004.001 -m 'Horizon v1.004.001'
git push github v1.004.001
gh run watch -R joaoalvess/horizon-ns2
```

## Integrar um núcleo novo (sob pedido)

1. `git fetch upstream --tags`
2. `git switch horizon && git merge --no-ff v<nova>`
3. Resolver os conflitos e conferir se a tabela "Pontos de contato" continua certa.
4. `git push github horizon:main`.
5. `contentVersion` `01.<novo patch>.001` e `UPSTREAM_CHECK_BASE_TAG` `v<nova>`; tag `v1.<novo patch>.001` e push da tag.
6. Atualizar a "Base atual" deste arquivo.

## Pontos de contato

| Arquivo do upstream | Mudança | Motivo |
|---|---|---|
| `sce_sys/param.json` | `contentVersion` no nosso esquema | A versão do app é a nossa |
| `headless/update_check/update_check.h`, `self_update.h` | `UPDATE_CHECK_HOST`, `UPDATE_CHECK_API`, `UPDATE_CHECK_MIRROR_API`, `SELF_UPDATE_API` | O updater consulta o nosso catálogo |
| `headless/update_check/self_update.c` | `self_update_keys` | Só aceita o catálogo assinado por nós |
| `headless/update_notice.cpp` | Arquivo da sequence `horizon-update-sequence` | Não mistura com a sequence do prosperostore |
| `headless/update_check_check.cpp` | URL esperada | Teste do upstream segue a URL nova |
| `headless/CMakeLists.txt` | `upstream_check.c` no objeto `update-check` | Aviso de núcleo novo |
| `headless/update_notice.cpp`, `update_notice.h` | `upstream_check` no fim do `Check` e `CoreNotice()` | Linha do núcleo em Configurações > Sobre |
| `README.md` | Seção `## Changes in v<versão>` | O `make-dist.py` tira dela as notas do release |
| `headless/settings_store.h`, `profiles.h`, `assets_dir.h`, `main.cpp`, `metadata_bridge.*`, `graphics.cpp`, `headless/eglantineps5/**` | Port do launcher do horizon-switch: plataformas, CNX, tema, arte, telas, categoria de patches | Etapas 2 a 5 |
| `Eglantine/src/core/arm/dynarmic/arm_dynarmic_64.cpp` | No PS5, `EDEN_JIT_COMPACT_EXITS` liga `compact_link_exits` (lido a cada JIT, não na inicialização estática) | Patch `compact_exits` |
| `Eglantine/src/dynarmic/src/dynarmic/backend/x64/a64_emit_x64.cpp`, `a64_emit_x64.h`, `interface/A64/config.h` | Links do `JitGroup` sem destino compilado saltam para um stub de saída por destino (`compact_link_exits`, desligado por padrão) | Patch `compact_exits` |
| `headless/main.cpp` | Include e uma linha depois do relatório de mods: `Horizon::Patches::Apply(title)` liga as chaves dos patches do jogo antes do `system.Load` | Etapa 5 |
| `headless/CMakeLists.txt`, `tools/build-headless-host.sh` | Alvo e execução do `eden-horizon-patches-check` | Etapa 5 |

## Etapas

| Etapa | O quê | Pronta quando | Status |
|---|---|---|---|
| 0. Base | Repo no GitHub, release com tag `-hN` e `check_package`; `make test` do upstream como aviso | Uma tag publica o ZIP no GitHub com lapy e `check_package` passando | Feita em 2026-11-24: `v1.000.004-h1` publicado em 14 min, lapy verificado, `check_package` PASS |
| 1. Update | Self-updater apontado para os nossos releases, manifesto assinado no CI, aviso de núcleo novo em Configurações > Sobre. Primeira instalação manual no console | Console com `01.004.001` vê o `01.004.002`, baixa e atualiza sozinho | Código na `horizon` (2026-11-24): chaves e catálogo nossos, `horizon-update.yml`, linha do núcleo em Configurações > Sobre. Falta: secret `HORIZON_UPDATE_KEY`, ligar o Pages, primeira instalação e teste no console |
| 2. Tradução | CNX do horizon-switch (`headless/cnx*`, `headless/mega*`, `tools/check-cnx.py`) em arquivos novos, instalando no `mods/` que o upstream já lê | `check-cnx.py` passa e um jogo abre em PT-BR no console | Código na `horizon` (`8019abe`, 2026-11-24); `check-cnx.py`: 327 checagens passaram no Mac. Falta testar no console |
| 3. Perfis | Biblioteca Switch 1 / Switch 2 (L1/R1) e configurações por plataforma (horizon-switch `32fd7bba`, `aea6cfbb`, `3bc00c6f`, `dd9a6ae7`) | Mudar o vídeo do Switch 2 não altera o do Switch 1 no console | Código na `horizon` (`319ea2c`, 2026-11-24), com a escolha de renderer do upstream mantida. Falta testar no console |
| 4. Visual | Launcher do horizon-switch (`fb298e97`..`6d4b6b6c`) em arquivos nossos | Prints do preview no Mac batem com o horizon-switch e foi conferido no console | Código na `horizon` (`96bee37`..`4c81133`, 2026-11-24); as telas de `pe/ui` passam no `-fsyntax-only` do Mac. Falta testar no console |
| 5. Patches | Registro de patches, interruptor por jogo, tabela de recomendados; primeiro patch medido no BotW | Desligado roda como o upstream; ligado melhora o FPS no console | Código na `horizon` (2026-11-24): registro (`headless/horizon_patches.h`), hook no `main.cpp`, categoria Patches Horizon nas configurações do jogo e patch `compact_exits`, desligado. Falta medir no console. O `host_call_return` (`8a8a67dc`) já está no upstream |

## Riscos conhecidos

- Um console com `v1.000.004-h1` ou com o app do upstream ainda consulta o prosperostore.net e oferece a versão do upstream, que apagaria a camada. A primeira instalação do `01.004.001` é manual.
- O `check_package.py` exige `titleId` `PPSA98515` e `titleName` `EglantinePS5`: renomear o app mexe nele.
- Dos 110 commits de núcleo do `rewrite/ps5-platform`, 52 não aplicavam limpo no `v1.000.004` (simulação de 2026-11-24). Patch de núcleo precisa ser pequeno no ponto de contato.

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
4. Versão: tag `v<versão do upstream>-hN` (ex.: `v1.000.004-h1`). O `N` sobe a cada release nosso e volta a 1 quando a base muda.

## Release

- Push no `main` do GitHub: roda o `make test` do upstream só como aviso (`.github/workflows/horizon-test.yml`, não trava nada). No `v1.000.004` ele não compila: `headless/memory_check.cpp:25` inclui `core/memory/read_traps.h` e `headless/nup_check.cpp` inclui `core/file_sys/nup.h`, que o núcleo não tem mais.
- Push de tag `v*-hN`: o `release.yml` builda o pacote (`make release`, que gera e confere o lapy e roda o `check_package.py`) e publica o pre-release se o build passar. Os nossos testes entram nesse gate a partir da etapa 2.
- Tempo: o primeiro build nosso, sem cache, levou 50 min; os de tag do upstream levaram de 40 a 66 min.

```fish
git push github horizon:main
git tag -a v1.000.004-h1 -m 'Horizon v1.000.004-h1'
git push github v1.000.004-h1
gh run watch -R joaoalvess/horizon-ns2
```

## Integrar um núcleo novo (sob pedido)

1. `git fetch upstream --tags`
2. `git switch horizon && git merge --no-ff v<nova>`
3. Resolver os conflitos e conferir se a tabela "Pontos de contato" continua certa.
4. `git push github horizon:main`.
5. Tag `v<nova>-h1` e push da tag.
6. Atualizar a "Base atual" deste arquivo.

## Pontos de contato

| Arquivo do upstream | Mudança | Motivo |
|---|---|---|
| `.github/workflows/release.yml` | Tag com sufixo `-hN` no "Match the tag" e no `publish` | O ZIP continua com o nome da versão do upstream |

## Etapas

| Etapa | O quê | Pronta quando | Status |
|---|---|---|---|
| 0. Base | Repo no GitHub, release com tag `-hN` e `check_package`; `make test` do upstream como aviso | Uma tag publica o ZIP no GitHub com lapy e `check_package` passando | Em andamento |
| 1. Update | Self-updater apontado para os nossos releases, manifesto assinado no CI, aviso de núcleo novo em Configurações > Sobre. Primeira instalação manual no console | Console com `-h1` vê o `-h2`, baixa e atualiza sozinho | |
| 2. Tradução | CNX do horizon-switch (`headless/cnx*`, `headless/mega*`, `tools/check-cnx.py`) em arquivos novos, instalando no `mods/` que o upstream já lê | `check-cnx.py` passa e um jogo abre em PT-BR no console | |
| 3. Perfis | Biblioteca Switch 1 / Switch 2 (L1/R1) e configurações por plataforma (horizon-switch `32fd7bba`, `aea6cfbb`, `3bc00c6f`, `dd9a6ae7`) | Mudar o vídeo do Switch 2 não altera o do Switch 1 no console | |
| 4. Visual | Launcher do horizon-switch (`fb298e97`..`6d4b6b6c`) em arquivos nossos | Prints do preview no Mac batem com o horizon-switch e foi conferido no console | |
| 5. Patches | Registro de patches, interruptor por jogo, tabela de recomendados; primeiro patch medido no BotW | Desligado roda como o upstream; ligado melhora o FPS no console | |

## Riscos conhecidos

- Até a etapa 1, o updater do app consulta o prosperostore.net pelo title ID `PPSA98515` e oferece a versão do upstream, que apagaria a camada.
- O `check_package.py` exige `titleId` `PPSA98515` e `titleName` `EglantinePS5`: renomear o app mexe nele.
- Dos 110 commits de núcleo do `rewrite/ps5-platform`, 52 não aplicavam limpo no `v1.000.004` (simulação de 2026-11-24). Patch de núcleo precisa ser pequeno no ponto de contato.

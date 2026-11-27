# 10 · Dados e apps opcionais (no cartão SD)

[⬅ Informações técnicas](09-informacoes-tecnicas.md) · [Índice](README.md) · [Próximo: compilar do código ➡](11-compilar-do-codigo.md)

O pacote de versão traz o **sistema inteiro** (interface, Multimídia, Livros, Arquivos,
Galeria, Terminal, GPS, Bluetooth, Telefone, FM, Ajustes…). Algumas partes usam arquivos
grandes ou programas compilados à parte, que ficam **no cartão SD**, fora da memória interna
(12 MB):

| Recurso | Pasta no cartão | Tamanho | Como obter |
|---|---|---|---|
| 🗺️ Mapas, rotas e busca do **Big GPS** | `BigFoston/big-gps/{mapas,rotas,busca}` | uma região: centenas de MB; Brasil: alguns GB | gerados no PC a partir do OpenStreetMap: [17-instalacao-sd.md](../big-gps/17-instalacao-sd.md) |
| 🗣️ Voz do Big GPS (Piper “Faber”, pt-BR) | `BigFoston/big-gps/voz/*.bgv` | frases fixas: 1,9 MB; com nomes de ruas do DF: 596 MB | `tools/big-gps/make-faber-voice.py SAIDA.bgv` no PC, com o modelo Piper `pt_BR-faber-medium` ([voz](../big-gps-2/22-piper-faber.md)) |
| 📺 **TV digital** | `BigFoston/tv/` | `bftvd` ~2,3 MB + firmware | `tools/bigfoston/build-tv.sh`; o firmware da Siano (`ISDBT.inp`) é proprietário e sai do **seu** aparelho: `tools/bigfoston/voice-extract.py OUT --tvfw ISDBT.inp` ([TV](../tv/00-visao-geral.md)) |
| 🌐 **Internet** (BigFoston Web, NetSurf 3.11) | `BigFoston/web/` | alguns MB | `tools/bigfoston/build-web.sh` ([navegador](../web-browser/00-overview.md)) |
| 🎮 **Big Remote Play** (jogos do PC) | `BigFoston/remoteplay/bfrp` + `BigFoston/web/lib` | ~1,5 MB | `tools/bigfoston/build-remoteplay.sh` gera também `big-remote-play.bfapp` ([Remote Play](../remote-play/00-overview.md)) |
| 🕹️ Apps `.bfapp` (SC Controller, Remote Play…) | `BigFoston/data/apps` | — | instalados pela própria interface ([pacotes](../jogos-e-pacotes/00-visao-geral.md)) |
| 📞 Voz das ligações | arquivos do DSP do próprio aparelho | — | [Telefone](../phone/00-visao-geral.md) |

Sem esses arquivos o BigFoston OS funciona normalmente; o app correspondente explica o que falta.

## Mapas do Big GPS em 3 comandos

Antes, uma vez: compile o construtor (`make -C tools/big-gps/builder`, bibliotecas em
[05-construtor-mapas.md](../big-gps/05-construtor-mapas.md)). Depois:

```bash
curl -LO https://download.geofabrik.de/south-america/brazil/centro-oeste-latest.osm.pbf
tools/big-gps/build-data.sh centro-oeste-latest.osm.pbf /tmp/dados centro-oeste
tools/big-gps/install-sd.py local /tmp/dados/big-gps /run/media/$USER/CARTAO
```

A gravação no cartão também é conferida por SHA-256 (relida do cartão, não do cache), e um
`MANIFEST.sha256` evita copiar de novo o que já está lá. Requisitos do construtor e o Brasil
inteiro: [05-construtor-mapas.md](../big-gps/05-construtor-mapas.md) e
[17-instalacao-sd.md](../big-gps/17-instalacao-sd.md). Dados © colaboradores do OpenStreetMap (ODbL).

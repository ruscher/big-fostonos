# 7 · Solução de problemas

[⬅ Atualizar](06-atualizar.md) · [Índice](README.md) · [Próximo: instalação avançada ➡](08-instalacao-avancada-usb.md)

## No computador (instalador)

| 😕 Sintoma | 🔍 Causa | ✅ O que fazer |
|---|---|---|
| “Os cartões não podem ser lidos” no topo | o UDisks2 não está rodando | `sudo systemctl enable --now udisks2` |
| O cartão não aparece na lista | não montável, ou está no FOSTON em modo serial | recoloque no leitor; **Não sabe qual é o seu cartão?** |
| Cartão listado em “não podem ser usados” | exFAT/NTFS, mais de 32 GB, trava de gravação | o motivo aparece na linha; use um SD/SDHC FAT32 |
| “Pacote inválido: SHA-256 não confere” | download corrompido | baixe o `.zip` de novo |
| “Pacote de outro aparelho” | pacote de outro modelo | use o pacote `BigFoston-OS-FS707DC-…` |
| “o que ficou no cartão não confere (cartão com defeito?)” | o cartão grava errado | **troque o cartão**; nada foi trocado no atual |
| “Espaço insuficiente no cartão” | cartão cheio | libere ~10 MB |
| “O volume mudou desde a escolha” | cartão trocado no meio | escolha o cartão de novo |
| Pede senha ao montar | política do polkit da distribuição | digite a senha do seu usuário (não é root) |

## No FOSTON

| 😕 Sintoma | 🔍 Causa | ✅ O que fazer |
|---|---|---|
| Navegação não abre nada | arquivo copiado pelo cabo sem reset (cache do CE) | **reset** no FOSTON, ou prepare pelo leitor de cartões |
| Agente: “O cartão não tem o BigFoston OS” | falta `BigFoston\update` no cartão | prepare o cartão com o instalador |
| `update.log`: **RECUSADA: … SHA-256 diferente** | cartão corrompeu os arquivos | **Verificar e reparar** no instalador; troque o cartão |
| `update.log`: **RECUSADA: fs707dc.dtb sem /chosen/bigfoston,boot-epoch** | DTB copiado do build, não do pacote | use o DTB do pacote (ou `linux-update.py stage`) |
| `update.log`: **INCOMPLETA** | faltou espaço ou energia no meio | desligue e ligue: continua de onde parou |
| “Linux da memória interna incompleto: ficando no Windows CE” | atualização interrompida | desligue e ligue com o cartão no aparelho |
| A contagem não aparece ao ligar | `Launch90` não gravado ou o agente não está na memória interna | abra o agente pelo cartão: se aparecer **Instalar o Dual Boot**, instale de novo |
| Tela preta ou com faixas no Linux | travamento | **reset**: volta ao Windows CE; relate com a versão |
| Aparelho desligou com `reboot -f` | watchdog do hardware | ligue pelo botão |
| O PC vê porta serial em vez de pendrive | o driver USB do CE guardou o modo serial | Ajustes → USB → MASS STORAGE |
| Sem hora certa no BigFoston OS | não há RTC no Linux; a hora vem do CE | acerte a hora no Windows CE, ou Ajustes → Data → **Acertar agora** (pela hora do GPS) |
| Bateria não carrega pelo USB do PC | o USB do PC não sustenta a carga (VCHG < 4,35 V) | use o carregador de tomada |

## Arquivos que ajudam no diagnóstico

| Onde | O que tem |
|---|---|
| cartão: `BigFoston/update.log` | APLICADA, RECUSADA ou INCOMPLETA, com o motivo |
| cartão: `BigFoston/recuperacao/original.txt` | os valores do registro antes da instalação |
| cartão: `BigFoston/logs/boot.log` | o log do kernel do último boot (45 s e 3 min) |
| aparelho: `\Temp\bfagent.log` | o que o agente fez desde que abriu |
| PC: relatório do instalador | **Concluído → Salvar relatório** |

Abra uma [issue](https://github.com/ruscher/big-fostonos/issues/new) com o sintoma, a versão
(`Ajustes → Sistema` ou o nome do `.zip`), foto da tela e os arquivos acima.

# 9 · Informações técnicas da instalação

[⬅ Instalação avançada](08-instalacao-avancada-usb.md) · [Índice](README.md) · [Próximo: dados e apps opcionais ➡](10-dados-e-apps-opcionais.md)

## O que muda no aparelho (e o que nunca muda)

| Onde | Muda? | O quê |
|---|:-:|---|
| EBoot (bootloader do fabricante) | ❌ nunca | `JWD B.13D.000000.101025.V2.0.6` intacto |
| Imagem do Windows CE (BINFS, 36 MB) | ❌ nunca | — |
| Partições do sistema e setores brutos da NAND | ❌ nunca | só arquivos, pelo sistema de arquivos do próprio CE |
| `\ResidentFlash\BigFoston\` (partição de dados, 12 MB) | ✅ criada | `bfagent.exe`, `autoboot.conf`, `original.txt`, `linux\{zImage, initrd.img, fs707dc.dtb, SHA256SUMS}` (~6,2 MB) |
| Registro persistente `HKLM\init` | ✅ 2 valores | `Launch90` = `\ResidentFlash\BigFoston\bfagent.exe`, `Depend90` = `14 00 32 00` |
| `HKCU\OEM\ShellEx\GpsPath` (Caminho da navegação) | 👆 você | só você troca, pelo menu Ajustes |
| `HKLM\Drivers\USB\FunctionDrivers\DefaultClientDriver` | ↩️ só devolve | o driver USB do CE às vezes grava o modo serial como padrão; o agente devolve `Mass_Storage_Class` |
| Cartão SD | ✅ pasta `BigFoston/` | nada fora dela |

## A cadeia de boot

```mermaid
sequenceDiagram
    autonumber
    participant E as EBoot
    participant CE as Windows CE 5.0
    participant A as bfagent (\ResidentFlash)
    participant K as Linux 6.12
    E->>CE: carrega a imagem do CE (BINFS)
    CE->>A: HKLM\init\Launch90 (depois de device.exe e do menu)
    A->>A: copia-se para \Temp e reabre (o original fica livre)
    A->>A: há BigFoston\update no cartão? confere SHA-256 e grava na memória interna
    A->>A: tela Dual Boot, contagem de 12 s
    alt toque em Windows CE
        A-->>CE: fecha: o CE segue normal
    else contagem termina / Iniciar agora
        A->>A: lê zImage, initrd e DTB; põe a hora do CE no DTB (bigfoston,boot-epoch)
        A->>K: páginas com PA conhecido, stub VA=PA, zera o FCSE PID, MMU off, salta
        K->>K: kernel 6.12 → /init → BusyBox → bigfoston-shell (~15 s)
    end
    Note over K,CE: reset (furinho) ou desligar: volta tudo ao Windows CE
```

O carregador é como o HaRET (que nunca suportou o MT3351): o agente prepara as páginas físicas,
desliga MMU e caches e entra no kernel com `r1 = 0xFFFFFFFF` (boot por device tree) e
`r2` = endereço do DTB. Layout na RAM:

| Região | Endereço físico | Máximo |
|---|---|---|
| zImage | `0x00800000` | 8 MB |
| initrd | `0x01000000` | 12 MB |
| DTB | `0x01F00000` | 64 KB |
| páginas do carregador | ≥ `0x02000000` | — |
| framebuffer (não usar) | `0x03E00030` | 800×480×2 |

Detalhes: [05-bootloader.md](../05-bootloader.md) · [04-linux-port.md](../04-linux-port.md) ·
[dual-boot.md](../dual-boot.md).

## O pacote de versão

`manifest.json` (formato 1):

```json
{
 "format": 1, "product": "BigFoston OS", "version": "0.6.5", "date": "2026-11-27",
 "channel": "stable", "device": "foston-fs707dc",
 "agent": {"file": "bfagent.exe", "sha256": "…", "size": 113152, "version": "0.8.0"},
 "linux": [{"file": "linux/zImage", "sha256": "…", "size": 2114336}, "…"],
 "notes": "…", "min_free_mb": 16
}
```

| Regra | Por quê |
|---|---|
| kernel, initrd, DTB e agente sempre do mesmo manifesto | nunca misturar peças de builds diferentes |
| só 6 nomes aceitos, sem `..` nem links | um `.zip` não escreve fora da pasta |
| tetos: agente 1 MB, zImage 8 MB, initrd 12 MB, DTB 64 KB | o agente recusa acima disso (`upd_max` em `bfagent.c`) |
| `make_release.py` recusa initrd com chave SSH do construtor | numa imagem pública ela daria root em todo FOSTON instalado |
| `make_release.py` recusa build de outra versão | o shell dentro da imagem precisa dizer “BigFoston OS <versão>” |
| DTB com `/chosen/bigfoston,boot-epoch` | o agente escreve ali a hora do CE (o Linux não tem RTC) |

## Por que pelo cartão, e não pelo cabo USB

| Medido no aparelho | Consequência |
|---|---|
| O volume fica montado nos dois lados no modo pendrive | o CE não vê arquivos novos até um reset |
| O driver serial do CE perde dados acima de ~1 KB sem leitura | o protocolo precisa de créditos e pedidos ≤ 8 KB |
| O link serial trava depois de transferências longas | é preciso replugar o cabo |
| Escrever vários MB pelo modo pendrive falhou | cópias grandes não são confiáveis |

Por isso o instalador **só prepara o cartão** num leitor (rápido, relido do cartão, sem cache)
e o **próprio CE copia** para a memória interna, no código de atualização que já roda em todo boot.

## Segurança

- O instalador roda como usuário comum; montar e desmontar passa pelo UDisks2/polkit.
- Nunca grava no disco do sistema, num disco fixo, fora de FAT, acima de 32 GB ou num volume
  que mudou de identidade desde a escolha.
- A imagem pública não tem chave SSH: o SSH fica fechado até você pôr a sua chave
  (build próprio, [11-compilar-do-codigo.md](11-compilar-do-codigo.md)).
- O agente só lê memória física por mapeamento **somente leitura**; não grava registradores
  nem setores brutos.

# InOrdine

InOrdine è un’app portatile per Windows x64 che mette in ordine i file di una cartella. L’anteprima mostra dove finirà ogni file. Durante il riordino, le icone si muovono verso le rispettive destinazioni.

## Come si usa

1. Avvia **InOrdine.exe** e scegli la cartella da organizzare. Puoi anche trascinarla nella finestra.
2. Scegli il criterio di riordino e controlla l’anteprima. **Dettagli** mostra l’elenco completo.
3. Premi **Riordina i file**. Per provare le animazioni con file di esempio, scegli **Prova la demo**.

| Criterio | Esempio |
| --- | --- |
| Estensione | `Verbale.pdf` finisce in `pdf` |
| Categoria | `Foto.jpg` finisce in `Immagini` con l’interfaccia in italiano |
| Mese di modifica | Un file modificato a maggio 2026 finisce in `2026-05` |

L’app organizza i file con estensione al primo livello. Le sottocartelle esistenti possono essere riutilizzate come destinazione. Quando un nome è già occupato puoi saltare il file oppure aggiungere un numero al nome in arrivo.

## Animazioni e impostazioni

Puoi seguire i file con le animazioni **Classica**, **Esplosione** e **Orbita**. L’ingranaggio apre le preferenze per lingua e colori e lo storico dei riordini. Lo storico registra i percorsi e i nomi finali nel profilo Windows, così puoi ritrovare anche un file rinominato.

Le lingue disponibili sono italiano, inglese, spagnolo e francese. Sono presenti quattro palette e un selettore di colori personalizzati.

## Compilazione

Su Windows, con `gcc` e `windres` di MinGW-w64 x64 nel PATH, esegui dalla cartella principale in PowerShell.

```powershell
.\Compila.cmd
```

Il risultato è `build\InOrdine.exe`. Per i dettagli sulla compilazione riproducibile su Linux x86-64 e sulle esclusioni applicate ai file, consulta la [guida tecnica](LEGGIMI.txt).

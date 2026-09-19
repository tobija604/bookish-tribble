Te štiri datoteke tukaj so potrebne, preden je installer/index.html uporaben:

  bootloader.bin   <- .pio/build/smartbox-esp32s3/bootloader.bin
  partitions.bin   <- .pio/build/smartbox-esp32s3/partitions.bin
  firmware.bin     <- .pio/build/smartbox-esp32s3/firmware.bin
  littlefs.bin     <- .pio/build/smartbox-esp32s3/littlefs.bin

NAJLAŽJA POT (priporočeno): pusti, da jih zgradi GitHub Actions.

  1. Ustvari nov repozitorij na GitHub-u in vanj potisni (push) celotno
     mapo tega projekta (smart-box/) na vejo "main".
  2. V nastavitvah repozitorija: Settings → Pages → Build and deployment
     → Source: nastavi na "GitHub Actions".
  3. Workflow .github/workflows/build-and-deploy-installer.yml se sproži
     samodejno ob vsakem pushu na main (GitHub-ov strežnik ima, za
     razliko od tega sandboxa, poln dostop do PlatformIO registra) —
     prevede firmware, zgradi te štiri datoteke in celotno installer/
     mapo objavi na "https://<uporabnik>.github.io/<repo>/".
  4. Ta objavljena stran je od takrat naprej TOČNO to, kar si prosil:
     odpreš jo v Chrome/Edge, priklopiš ploščo prek USB-C, klikneš
     "NAMESTI SMART BOX" in izbereš serijska vrata — brskalnik sam
     prek Web Serial zapiše firmware na čip. Nič PlatformIO na tvojem
     računalniku ni več potrebno.
  5. Ob vsaki naslednji spremembi kode samo znova pushneš na main —
     stran se samodejno na novo zgradi in objavi.

ROČNA POT (če ne uporabljaš GitHub-a): na svojem računalniku, kjer imaš
normalen dostop do interneta, zaženi

  pio run -e smartbox-esp32s3
  pio run -e smartbox-esp32s3 -t buildfs

nato te štiri datoteke ročno prekopiraj sem, in celotno installer/ mapo
postrezi kot statično spletno stran (npr. `npx serve installer`, Netlify,
Vercel, GitHub Pages — mora biti HTTPS ali localhost, Web Serial na navadnem
http:// ne dela).

Ta mapa je v dostavljenem skeletonu namerno prazna — glej
docs/HARDWARE_NOTES.md, zakaj firmware v tem okolju (Claude sandbox) ni
mogel biti dejansko preveden.

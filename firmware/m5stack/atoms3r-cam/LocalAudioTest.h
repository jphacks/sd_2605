#pragma once
// Explicit USB-only provisioning aid, no network required. Replaced by next API sync.
inline bool createTestCache(Storage& storage) {
  StorageGuard guard(storage);
  if (!storage.mounted) return false;
  for (int slot = 0; slot < 2; ++slot) {
    const int bank = 1 - storage.records[slot].bank;
    const uint8_t header[] = {
      'R','I','F','F', 0x64,0x1f,0,0, 'W','A','V','E',
      'f','m','t',' ', 16,0,0,0, 1,0,1,0, 0x80,0x3e,0,0,
      0,0x7d,0,0, 2,0,16,0, 'd','a','t','a', 0x40,0x1f,0,0
    }; // 4000 samples / 16000 Hz = 250 ms, PCM16 mono.
    String path = storage.path(slot, bank);
    File f = LittleFS.open(path, FILE_WRITE);
    if (!f || f.write(header, sizeof(header)) != sizeof(header)) return false;
    uint8_t chunk[256];
    for (unsigned start = 0; start < 4000; start += 128) {
      unsigned samples = (4000 - start < 128) ? 4000 - start : 128;
      for (unsigned i = 0; i < samples; ++i) {
        int16_t value = ((start + i) / (slot ? 10 : 20)) % 2 ? 1800 : -1800;
        chunk[2*i] = uint16_t(value) & 255;
        chunk[2*i+1] = uint16_t(value) >> 8;
      }
      if (f.write(chunk, samples * 2) != samples * 2) return false;
    }
    f.close();
    f = LittleFS.open(path, FILE_READ);
    bool valid = f && validWav(f);
    f.close();
    if (!valid || !storage.commitAudio(slot, bank, -1)) return false;
  }
  return true;
}

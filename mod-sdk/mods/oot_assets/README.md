# OoT Asset Loader

Shared resources for OoT items, medallions and spiritual stones in MM. Extracts from a personal OoT NTSC 1.0/1.1/1.2 ROM, with z64/n64/v64 byte orders. Open Enhancements > OoT Asset Loader to extract again. The game asks before reading the chosen ROM and creating the archive; restart to mount it.

XML File OutNames isolate the extracted assets as textures/oot_*, objects/oot_*, overlays/oot_* and misc/oot_*. Shared 32px icons and combined jewel/medallion get-item models are included in this mod. Existing MM resources keep their names.

An already extracted personal oot.o2r can be imported with mod-sdk/tools/import_oot_assets.py --source <oot.o2r> --output <mods/oot.o2r>. The importer rewrites both display-list CRC references and serialized skeleton/animation paths. This installation uses the user's existing extracted OoT archive: 2,745 resources, ZIP integrity checked and references verified. OoT NTSC 1.0 extraction was verified using the normalized ROM left by the failed extraction attempt: 2,745 resources, all eight loader markers and ZIP integrity passed. NTSC 1.1/1.2 extraction remains unverified. The MM extractor now selects YAR decoding by the ROM CRC, and the host removes foreign ROM version metadata from secondary mod asset archives.

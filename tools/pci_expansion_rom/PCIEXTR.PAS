{
}

{
  #!/bin/bash
  # apt-get install fpc
  # nano /usr/bin/tpc

  fpc $1.PAS ; ./$1
}


program PCIExpansionROMExtractor;


uses crt;


type
  {$PackRecords 1}
  PnPOptionROMHeader = record
    signature:       array [1..2] of byte;
    blockCount:      byte;
    initEntryCode:   array [1..4] of byte;
    reservedData:    array [1..17] of byte;
    pciDataOffset:   uint16;
    expHeaderOffset: uint16
  end;
  {$PackRecords default}

  {$PackRecords 1}
  PCIDataHeader = record
    signature:       array [1..4] of byte;
    vendorId:        uint16;
    deviceId:        uint16;
    vpdPtr:          uint16;
    structLen:       uint16;
    revision:        byte;
    classCode:       byte;                     {Base class, 02h -> Network}
    subClassCode:    byte;                     {Sub class, 0}
    interfaceCode:   byte;                     {Interface, 0}  {-> Network}
    imageBlockCount: uint16;
    revLevel:        uint16;
    codeType:        byte;                     {x86 or x64}
    indicator:       byte;                     {is last image}
    reserved:        uint16;
  end;
  {$PackRecords default}

  {$PackRecords 1}
  ExpansionHeader = record
    signature:       array [1..4] of byte;
    structRevision:  byte;
    length:          byte;                     {Header len (mul of 16 bytes)}
    nextHeaderOfset: uint16;                   {00 if none}
    reserve0:        byte;
    checkSum:        byte;                     {for this header only}
    longDeviceId:    uint32;                   {PnP device ID -> 0 for Realtek}
    manufacturePtr:  uint16;
    productPtr:      uint16;
    deviceTypeCode:  array [1..3] of byte;
    deviceIndicator: byte;                     {See page 18 of PnP BIOS spec}
    bcv:             uint16;
    dv:              uint16;
    bev:             uint16;
    reserve1 :       byte;
    staticResPtr:    uint16;                   {0 if none}
  end;
  {$PackRecords default}

  ByteArray = array [0..0] of byte;
  ByteArrayPtr = ^ByteArray;


var
  romFile:     file of PnPOptionROMHeader;
  romHeader:   PnPOptionROMHeader;


function toHexStr(b: byte): string;
  const
    HEX_DIGITS = '0123456789ABCDEF';
  var
    {result: string;}
    hiPart: integer;
    loPart: integer;
begin
  hiPart    := b div 16;
  loPart    := b mod 16;
  toHexStr  := HEX_DIGITS[hiPart + 1] + HEX_DIGITS[loPart + 1];
end;

procedure writeUsage;
begin
  writeln('Usage: ');
  writeln('       ', ParamStr(0), ' <rom file>')
end;

procedure extractRom(romFileName: string);
  procedure writeCaption(caption: string);
    const
      CAPTION_WIDTH = 26;
    var
      padCount: integer;    
  begin
    padCount := CAPTION_WIDTH - ord(caption[0]);
    write(#9, caption, ' ':padCount)
  end;

  procedure dumpBytes(
    fileName: string;
    nBuffer: integer;
    pBuffer: ByteArrayPtr
  );
    var
      f:  file;
  begin
    assign(f, fileName);
    rewrite(f, 1);
    blockwrite(f, pBuffer^[0], nBuffer);
    close(f)
  end;

  procedure readBytes(
    fileName: string;
    offset: integer;
    nBuffer: integer;
    pBuffer: ByteArrayPtr
  );
    var
      f:  file;
  begin
    assign(f, fileName);
    reset(f, 1);
    seek(f, offset);
    blockread(f, pBuffer^[0], nBuffer);
    close(f)
  end;

  var
    i:         integer;
    pPCIData:  PCIDataHeader;
    expansion: ExpansionHeader;
begin
  writeln('Rom file: ', romFileName);
  assign(romFile, romFileName);
  reset(romFile);
  read(romFile, romHeader);

  with romHeader do
  begin
    writeCaption('Signature:');
    writeln('[', toHexStr(signature[1]), ' ', toHexStr(signature[2]), ']');

    writeCaption('Blocks:');
    writeln(blockCount, ' (', blockCount * 512, ' bytes)');

    writeCaption('Init entry point:');
    writeln('file ''init.bin''');
    dumpBytes('init.bin', 4, @initEntryCode);

    writeCaption('PCI data offset:');
    writeln(pciDataOffset, ' (file ''pci.bin'')');
    readBytes(romFileName, pciDataOffset, sizeOf(pPCIData), @pPCIData);
    dumpBytes('pci.bin', sizeOf(pPCIData), @pPCIData);
    with pPCIData do
    begin
      writeCaption('    Signature:');

      for i:= 1 to 4 do
      begin
        write(char(signature[i]));
      end;
      writeln();

      writeCaption('    Vid:');
      writeln('[', toHexStr(vendorId), ']');
      writeCaption('    Did:');
      writeln('[', toHexStr(deviceId), ']');
      writeCaption('    ^VPD:');
      writeln('[', toHexStr(vpdPtr div 256), ' ' ,toHexStr(vpdPtr mod 256), ']');
      writeCaption('    Length:');
      writeln(structLen);
      writeCaption('    Rev:');
      writeln(revision);
      writeCaption('    Code:');
      writeln();
      writeCaption('        Class:');
      writeln(classCode);
      writeCaption('        SubClass:');
      writeln(subClassCode);
      writeCaption('        Interface:');
      writeln(interfaceCode);
      writeCaption('    Img blk count:');
      writeln(imageBlockCount);
      writeCaption('    Revision lv:');
      writeln(revLevel);
      writeCaption('    Code type:');
      if codeType = 0 then
        writeln(codeType, ' (Intel x86)')
      else
        writeln(codeType);
      writeCaption('    Indicator:');
      writeln('[', toHexStr(indicator), ']');
    end;

    writeCaption('Expansion header offset:');
    writeln(expHeaderOffset, ' (file ''expansion.bin'')');
    if expHeaderOffset > 0 then
    begin
      readBytes(romFileName, expHeaderOffset, sizeOf(expansion), @expansion);
      dumpBytes('expansion.bin', sizeOf(expansion), @expansion);
      with expansion do
      begin
        writeCaption('    Signature:');

        for i:= 1 to 4 do
        begin
          write(char(signature[i]));
        end;
        writeln();

        writeCaption('    Struct rev:');
        writeln(structRevision);
        writeCaption('    Length:');
        writeln(length);
        writeCaption('    Next Header Ofset:');
        writeln(nextHeaderOfset);
        writeCaption('    checkSum:');
        writeln(checkSum);
        writeCaption('    longDeviceId:');
        writeln(longDeviceId);
        writeCaption('    manufacturePtr:');
        writeln(manufacturePtr);
        writeCaption('    productPtr:');
        writeln(productPtr);
        writeCaption('    Device type code:');
        writeln(
          '[',
          toHexStr(deviceTypeCode[1]), ' ',
          toHexStr(deviceTypeCode[2]), ' ',
          toHexStr(deviceTypeCode[3]),
          ']'
        );
        writeCaption('    deviceIndicator:');
        writeln(deviceIndicator);
        writeCaption('    bcv:');
        writeln(bcv);
        writeCaption('    dv:');
        writeln(dv);
        writeCaption('    bev:');
        writeln(bev);
        writeCaption('    staticResPtr:');
        writeln(staticResPtr);
      end
    end
  end
end;


begin
  if ParamCount = 1 then
    extractRom(ParamStr(1))
  else
    writeUsage
end.



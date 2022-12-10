// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Top vertex loaders
// Metroid Prime: P I16-flt N I16-s16 T0 I16-u16 T1 i16-flt

#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoader;
typedef void (*TPipelineFunction)(VertexLoader* loader);

class VertexLoader : public VertexLoaderBase
{
public:
  VertexLoader(const TVtxDesc& vtx_desc, const VAT& vtx_attr);

  int RunVertices(const u8* src, u8* dst, int count) override;
  // They are used for the communication with the loader functions
  float m_posScale;
  float m_tcScale[8];
  int m_tcIndex;
  int m_colIndex;

  // Matrix components are first in GC format but later in PC format - we need to store it
  // temporarily
  // when decoding each vertex.
  u8 m_curtexmtx[8];
  int m_texmtxwrite;
  int m_texmtxread;
  bool m_vertexSkip;
  int m_skippedVertices;
  int m_remaining;

private:
  // Pipeline.
  TPipelineFunction m_PipelineStages[64];  // TODO - figure out real max. it's lower.
  int m_numPipelineStages;

  void CompileVertexTranslator();

  void WriteCall(TPipelineFunction);
};

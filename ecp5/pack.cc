/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  David Shah <david@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <algorithm>
#include <iterator>
#include <unordered_set>
#include "cells.h"
#include "design_utils.h"
#include "log.h"
#include "util.h"

NEXTPNR_NAMESPACE_BEGIN

static bool is_nextpnr_iob(Context *ctx, CellInfo *cell)
{
    return cell->type == ctx->id("$nextpnr_ibuf") || cell->type == ctx->id("$nextpnr_obuf") ||
           cell->type == ctx->id("$nextpnr_iobuf");
}

class Ecp5Packer
{
  public:
    Ecp5Packer(Context *ctx) : ctx(ctx){};

  private:
    // Process the contents of packed_cells and new_cells
    void flush_cells()
    {
        for (auto pcell : packed_cells) {
            ctx->cells.erase(pcell);
        }
        for (auto &ncell : new_cells) {
            ctx->cells[ncell->name] = std::move(ncell);
        }
        packed_cells.clear();
        new_cells.clear();
    }

    // Simple "packer" to remove nextpnr IOBUFs, this assumes IOBUFs are manually instantiated
    void pack_io()
    {
        log_info("Packing IOs..\n");

        for (auto cell : sorted(ctx->cells)) {
            CellInfo *ci = cell.second;
            if (is_nextpnr_iob(ctx, ci)) {
                CellInfo *trio = nullptr;
                if (ci->type == ctx->id("$nextpnr_ibuf") || ci->type == ctx->id("$nextpnr_iobuf")) {
                    trio = net_only_drives(ctx, ci->ports.at(ctx->id("O")).net, is_trellis_io, ctx->id("B"), true, ci);

                } else if (ci->type == ctx->id("$nextpnr_obuf")) {
                    trio = net_only_drives(ctx, ci->ports.at(ctx->id("I")).net, is_trellis_io, ctx->id("B"), true, ci);
                }
                if (trio != nullptr) {
                    // Trivial case, TRELLIS_IO used. Just destroy the net and the
                    // iobuf
                    log_info("%s feeds TRELLIS_IO %s, removing %s %s.\n", ci->name.c_str(ctx), trio->name.c_str(ctx),
                             ci->type.c_str(ctx), ci->name.c_str(ctx));
                    NetInfo *net = trio->ports.at(ctx->id("B")).net;
                    if (net != nullptr) {
                        ctx->nets.erase(net->name);
                        trio->ports.at(ctx->id("B")).net = nullptr;
                    }
                    if (ci->type == ctx->id("$nextpnr_iobuf")) {
                        NetInfo *net2 = ci->ports.at(ctx->id("I")).net;
                        if (net2 != nullptr) {
                            ctx->nets.erase(net2->name);
                        }
                    }
                } else {
                    log_error("TRELLIS_IO required on all top level IOs...\n");
                }
                packed_cells.insert(ci->name);
                std::copy(ci->attrs.begin(), ci->attrs.end(), std::inserter(trio->attrs, trio->attrs.begin()));
            }
        }
        flush_cells();
    }

    // Pass to pack LUT5s into a newly created slice
    void pack_lut5s()
    {
        log_info("Packing LUT5s...\n");
        for (auto cell : sorted(ctx->cells)) {
            CellInfo *ci = cell.second;
            if (is_pfumx(ctx, ci)) {
                std::unique_ptr<CellInfo> packed =
                        create_ecp5_cell(ctx, ctx->id("TRELLIS_SLICE"), ci->name.str(ctx) + "_SLICE");
                NetInfo *f0 = ci->ports.at(ctx->id("BLUT")).net;
                if (f0 == nullptr)
                    log_error("PFUMX '%s' has disconnected port 'BLUT'\n", ci->name.c_str(ctx));
                NetInfo *f1 = ci->ports.at(ctx->id("ALUT")).net;
                if (f1 == nullptr)
                    log_error("PFUMX '%s' has disconnected port 'ALUT'\n", ci->name.c_str(ctx));
                CellInfo *lc0 = net_driven_by(ctx, f0, is_lut, ctx->id("Z"));
                CellInfo *lc1 = net_driven_by(ctx, f1, is_lut, ctx->id("Z"));
                if (lc0 == nullptr)
                    log_error("PFUMX '%s' has BLUT driven by cell other than a LUT\n", ci->name.c_str(ctx));
                if (lc1 == nullptr)
                    log_error("PFUMX '%s' has ALUT driven by cell other than a LUT\n", ci->name.c_str(ctx));
                replace_port(lc0, ctx->id("A"), packed.get(), ctx->id("A0"));
                replace_port(lc0, ctx->id("B"), packed.get(), ctx->id("B0"));
                replace_port(lc0, ctx->id("C"), packed.get(), ctx->id("C0"));
                replace_port(lc0, ctx->id("D"), packed.get(), ctx->id("D0"));
                replace_port(lc1, ctx->id("A"), packed.get(), ctx->id("A1"));
                replace_port(lc1, ctx->id("B"), packed.get(), ctx->id("B1"));
                replace_port(lc1, ctx->id("C"), packed.get(), ctx->id("C1"));
                replace_port(lc1, ctx->id("D"), packed.get(), ctx->id("D1"));
                replace_port(ci, ctx->id("C0"), packed.get(), ctx->id("M0"));
                replace_port(ci, ctx->id("Z"), packed.get(), ctx->id("OFX0"));
                ctx->nets.erase(f0->name);
                ctx->nets.erase(f1->name);
                new_cells.push_back(std::move(packed));
                packed_cells.insert(lc0->name);
                packed_cells.insert(lc1->name);
                packed_cells.insert(ci->name);
            }
        }
        flush_cells();
    }

  public:
    void pack()
    {
        pack_io();
        pack_lut5s();
    }

  private:
    Context *ctx;

    std::unordered_set<IdString> packed_cells;
    std::vector<std::unique_ptr<CellInfo>> new_cells;

    struct SliceUsage
    {
        bool lut0_used = false, lut1_used = false;
        bool ccu2_used = false, dpram_used = false, ramw_used = false;
        bool ff0_used = false, ff1_used = false;
        bool mux5_used = false, muxx_used = false;
    };

    std::unordered_map<IdString, SliceUsage> sliceUsage;
};

// Main pack function
bool Arch::pack()
{
    Context *ctx = getCtx();
    try {
        log_break();
        Ecp5Packer(ctx).pack();
        log_info("Checksum: 0x%08x\n", ctx->checksum());
        return true;
    } catch (log_execution_error_exception) {
        return false;
    }
}

NEXTPNR_NAMESPACE_END

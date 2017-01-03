#include "ops/op.h"

#include <stddef.h>  // offsetof

#include "helpers.h"

#include "ops/constants.h"
#include "ops/controlflow.h"
#include "ops/delay.h"
#include "ops/hardware.h"
#include "ops/maths.h"
#include "ops/metronome.h"
#include "ops/patterns.h"
#include "ops/queue.h"
#include "ops/stack.h"
#include "ops/variables.h"


/////////////////////////////////////////////////////////////////
// OPS //////////////////////////////////////////////////////////

const tele_op_t *tele_ops[OPS] = {
    // variables
    &op_A, &op_B, &op_C, &op_D, &op_DRUNK, &op_DRUNK_MAX, &op_DRUNK_MIN,
    &op_DRUNK_WRAP, &op_FLIP, &op_I, &op_IN, &op_O, &op_O_INC, &op_O_MAX,
    &op_O_MIN, &op_O_WRAP, &op_PARAM, &op_T, &op_TIME, &op_TIME_ACT, &op_X,
    &op_Y, &op_Z,

    // metronome
    &op_M, &op_M_ACT, &op_M_RESET,

    // patterns
    &op_P, &op_P_HERE, &op_P_END, &op_P_I, &op_P_L, &op_P_N, &op_P_NEXT,
    &op_P_PREV, &op_P_START, &op_P_WRAP, &op_P_INS, &op_P_RM, &op_P_PUSH,
    &op_P_POP, &op_PN,

    // queue
    &op_Q, &op_Q_AVG, &op_Q_N,

    // hardware
    &op_CV, &op_CV_OFF, &op_CV_SLEW, &op_TR, &op_TR_POL, &op_TR_TIME,
    &op_TR_TOG, &op_TR_PULSE, &op_II, &op_CV_SET, &op_MUTE, &op_UNMUTE,
    &op_STATE, &op_JF_TR, &op_JF_RMODE, &op_JF_RUN, &op_JF_SHIFT, &op_JF_VTR,
    &op_JF_MODE, &op_JF_TICK, &op_JF_VOX, &op_JF_NOTE, &op_JF_GOD, &op_JF_TUNE,
    &op_JF_QT, &op_KR_PRESET, &op_KR_PATTERN, &op_KR_SCALE, &op_KR_PERIOD,
    &op_KR_POS, &op_KR_LOOP_ST, &op_KR_LOOP_LEN, &op_KR_RESET, &op_MP_PRESET1,
    &op_MP_RESET1, &op_MP_STOP1, &op_MP_SCALE, &op_MP_PERIOD, &op_LV_PRESET,
    &op_LV_RESET, &op_LV_POS, &op_LV_L_ST, &op_LV_L_LEN, &op_LV_L_DIR,
    &op_CY_PRESET, &op_CY_RESET, &op_CY_POS, &op_CY_REV, &op_ARP_STYLE,
    &op_ARP_HOLD, &op_ARP_STEPS, &op_ARP_DIST, &op_ARP_GATE, &op_ARP_DIV,

    // maths
    &op_ADD, &op_SUB, &op_MUL, &op_DIV, &op_MOD, &op_RAND, &op_RRAND, &op_TOSS,
    &op_MIN, &op_MAX, &op_LIM, &op_WRAP, &op_QT, &op_AVG, &op_EQ, &op_NE,
    &op_LT, &op_GT, &op_NZ, &op_EZ, &op_RSH, &op_LSH, &op_EXP, &op_ABS, &op_AND,
    &op_OR, &op_XOR, &op_JI, &op_SCALE, &op_N, &op_V, &op_VV, &op_ER,

    // stack
    &op_S_ALL, &op_S_POP, &op_S_CLR, &op_S_L,

    // controlflow
    &op_SCRIPT, &op_KILL, &op_SCENE,

    // delay
    &op_DEL_CLR,

    // constants
    &op_WW_PRESET, &op_WW_POS, &op_WW_SYNC, &op_WW_START, &op_WW_END,
    &op_WW_PMODE, &op_WW_PATTERN, &op_WW_QPATTERN, &op_WW_MUTE1, &op_WW_MUTE2,
    &op_WW_MUTE3, &op_WW_MUTE4, &op_WW_MUTEA, &op_WW_MUTEB, &op_MP_PRESET,
    &op_MP_RESET, &op_MP_SYNC, &op_MP_MUTE, &op_MP_UNMUTE, &op_MP_FREEZE,
    &op_MP_UNFREEZE, &op_MP_STOP, &op_ES_PRESET, &op_ES_MODE, &op_ES_CLOCK,
    &op_ES_RESET, &op_ES_PATTERN, &op_ES_TRANS, &op_ES_STOP, &op_ES_TRIPLE,
    &op_ES_MAGIC, &op_ORCA_TRK, &op_ORCA_CLK, &op_ORCA_DIV, &op_ORCA_PHASE,
    &op_ORCA_RST, &op_ORCA_WGT, &op_ORCA_MUTE, &op_ORCA_SCALE, &op_ORCA_BANK,
    &op_ORCA_PRESET, &op_ORCA_RELOAD, &op_ORCA_ROTS, &op_ORCA_ROTW,
    &op_ORCA_GRST, &op_ORCA_CVA, &op_ORCA_CVB,
};

/////////////////////////////////////////////////////////////////
// MODS /////////////////////////////////////////////////////////

const tele_mod_t *tele_mods[MODS] = {
    // controlflow
    &mod_IF, &mod_ELIF, &mod_ELSE, &mod_L, &mod_PROB,

    // delay
    &mod_DEL,

    // stack
    &mod_S

};

/////////////////////////////////////////////////////////////////
// HELPERS //////////////////////////////////////////////////////

void op_constant(const void *data, scene_state_t *NOTUSED(ss),
                 exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, (intptr_t)data);
}

void op_peek_i16(const void *data, scene_state_t *ss, exec_state_t *NOTUSED(es),
                 command_state_t *cs) {
    char *base = (char *)ss;
    size_t offset = (size_t)data;
    int16_t *ptr = (int16_t *)(base + offset);
    cs_push(cs, *ptr);
}

void op_poke_i16(const void *data, scene_state_t *ss, exec_state_t *NOTUSED(es),
                 command_state_t *cs) {
    char *base = (char *)ss;
    size_t offset = (size_t)data;
    int16_t *ptr = (int16_t *)(base + offset);
    *ptr = cs_pop(cs);
}

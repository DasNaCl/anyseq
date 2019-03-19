

#include "align.h"

#define MATCH       2
#define MISMATCH   -1
#define GAPPENALTY -1

#ifndef max
#define max(a, b) (((a) < (b)) ? (b) : (a))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define HI_1J_OLD_IDX    0
#define HI_1J_1_OLD_IDX  1
#define HIJ_1_OLD_IDX    2
#define MAXIJ_OLD_IDX    3
#define PE_STORAGE_SIZE  4

#define IAI_IDX          0
#define IHI_1J_IDX       1
#define IMAXI_1J_IDX     2
#define PE_INPUT_SIZE    3

#define SBT_A            0
#define SBT_C            1
#define SBT_G            2
#define SBT_T            3
#define SBT_COL_SIZE     4   // A, G, T, C

/// ONLY LOCAL ALIGNMENTS
//// note that |buffer| == subject_len
void align(hls::stream<InputStreamType>& sub, SequenceElem query[PE_COUNT], int s_len, int q_len,
		   hls::stream<OutputStreamType>& wbuff, ScoreType* res)
{
#pragma HLS INTERFACE axis off port=sub
#pragma HLS STREAM variable=sub dim=1

#pragma HLS INTERFACE axis off port=wbuff
#pragma HLS STREAM variable=wbuff dim=1
#pragma HLS INTERFACE s_axilite depth=16 port=query bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=s_len bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=q_len bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=res bundle=CTRL
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL

	int subject_len = s_len;
	int query_len = q_len;

	// local memory
	ScoreType scoring[pe_count][PE_STORAGE_SIZE];
#pragma HLS ARRAY_PARTITION variable=scoring block factor=4 dim=1
	ScoreType inputs[pe_count][PE_INPUT_SIZE];
#pragma HLS ARRAY_PARTITION variable=inputs block factor=4 dim=1
	SequenceElem query_cpy[pe_count];
#pragma HLS ARRAY_PARTITION variable=query_cpy block factor=4 dim=1

	// initialization
	readQuery: for(int pe = 0; pe < pe_count; pe++) {
#pragma HLS UNROLL
		query_cpy[pe] = query[pe];
		// init inputs
		inputs[pe][IAI_IDX] = SBT_COL_SIZE;
		inputs[pe][IHI_1J_IDX] = inputs[pe][IMAXI_1J_IDX] = 0;
		// init scoring    note: initialization is different for other PEs
		scoring[pe][HI_1J_OLD_IDX] = scoring[pe][HI_1J_1_OLD_IDX]
				= scoring[pe][HIJ_1_OLD_IDX] = scoring[pe][MAXIJ_OLD_IDX] = 0;
	}
	//// Compute the score
	int old_idx = 0;
	const int steps = subject_len + max(pe_count, query_len) - 1;
	InputStreamType olds[PE_COUNT];
#pragma HLS ARRAY_PARTITION variable=olds block=factor=4 dim=1
	systolic0: for(int i = 0; i < steps; i++) {
#pragma HLS LOOP_TRIPCOUNT min=4699745 max=47000641 avg=4700641
									// max=s_len * K_pe - 1 =avg
#pragma HLS PIPELINE II=1
		if(i < subject_len) {
			InputStreamType data = sub.read(); //[que_sub_it++];
			inputs[0][IAI_IDX] = data.data >> 16;
			inputs[0][IHI_1J_IDX] = (data.data << 16) >> 16;
			olds[i % PE_COUNT] = data;
		}
		else {
			inputs[0][IAI_IDX] = SBT_COL_SIZE;
			inputs[0][IHI_1J_IDX] = 0;
		}

		systolic1: for(int pe = pe_count-1; pe >= 0; pe--) {
#pragma HLS LOOP_FLATTEN
			const ScoreType iai = inputs[pe][IAI_IDX];

			ScoreType m1 = 0;
			if(pe < query_len && iai < SBT_COL_SIZE) {
				m1 = scoring[pe][HI_1J_1_OLD_IDX] + (iai == query_cpy[pe] ? MATCH : MISMATCH);
			}
			// relax
			const int ihi_1jbuf = inputs[pe][IHI_1J_IDX];
			const int hij_1_old = scoring[pe][HIJ_1_OLD_IDX];
			const ScoreType m2 = GAPPENALTY + max(hij_1_old, ihi_1jbuf);
			const ScoreType hij = max(ScoreType(0), max(m1, m2));

			const ScoreType maxij = max(hij, max(inputs[pe][IMAXI_1J_IDX], scoring[pe][MAXIJ_OLD_IDX]));
			// update my data
			scoring[pe][HI_1J_1_OLD_IDX] = ihi_1jbuf;
			scoring[pe][HIJ_1_OLD_IDX] = iai < SBT_COL_SIZE ? hij : hij_1_old;
			scoring[pe][MAXIJ_OLD_IDX] = maxij;
			// set input
			if(pe + 1 < pe_count)
			{
				inputs[pe + 1][IAI_IDX] = iai;
				inputs[pe + 1][IHI_1J_IDX] = hij;
				inputs[pe + 1][IMAXI_1J_IDX] = maxij;
			}
		}
		if(i >= pe_count - 1) {
			// write stuff to buffer
			//wbuff[i - pe_count + 1] = (scoring[pe_count - 1][HIJ_1_OLD_IDX]);
			OutputStreamType store;
			InputStreamType oldstor = olds[old_idx];
			old_idx = (old_idx >= pe_count - 1 ? 0 : old_idx + 1);

			store.data = scoring[pe_count - 1][HIJ_1_OLD_IDX];
			store.last = (old_idx == 0 || i + 1 >= steps);

			store.dest = oldstor.dest;
			store.id = oldstor.id;
			store.keep = oldstor.keep;
			store.strb = oldstor.strb;
			store.user = oldstor.user;

			wbuff.write(store);
		}
	}
	*res = scoring[pe_count - 1][MAXIJ_OLD_IDX];
}


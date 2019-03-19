

#include "align_glob.h"

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
#define PE_STORAGE_SIZE  3

#define IAI_IDX          0
#define IHI_1J_IDX       1
#define PE_INPUT_SIZE    2

#define INV_CHAR 4

void align(hls::stream<InputStreamType>& sub, SequenceElem query[PE_COUNT], int s_len, int q_len,
		   hls::stream<OutputStreamType>& wbuff, ScoreType* res, ScoreType top_row[PE_COUNT])
{
#pragma HLS INTERFACE axis off port=sub
#pragma HLS STREAM variable=sub dim=1

#pragma HLS INTERFACE axis off port=wbuff
#pragma HLS STREAM variable=wbuff dim=1
#pragma HLS INTERFACE s_axilite depth=896 port=query bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=s_len bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=q_len bundle=CTRL
#pragma HLS INTERFACE s_axilite register depth=1 port=res bundle=CTRL
#pragma HLS INTERFACE s_axilite depth=896 port=top_row bundle=CTRL
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL

	int subject_len = s_len;
	int query_len = q_len;
	// local memory
	ScoreType scoring[pe_count][PE_STORAGE_SIZE];
#pragma HLS ARRAY_PARTITION variable=scoring block factor=224 dim=1
	ScoreType inputs[pe_count][PE_INPUT_SIZE];
#pragma HLS ARRAY_PARTITION variable=inputs block factor=224 dim=1
	SequenceElem query_cpy[pe_count];
#pragma HLS ARRAY_PARTITION variable=query_cpy block factor=224 dim=1
  bool degenerate[pe_count];
#pragma HLS ARRAY_PARTITION variable=degenerate block factor=224 dim=1

	// initialization
	readQuery: for(int pe = 0; pe < pe_count; pe++) {
#pragma HLS UNROLL
		query_cpy[pe] = query[pe];
		// init inputs
		inputs[pe][IAI_IDX] = INV_CHAR;
		inputs[pe][IHI_1J_IDX] = 0;
    scoring[pe][HI_1J_1_OLD_IDX] = top_row[pe];
		// init scoring    note: initialization is different for other PEs
		scoring[pe][HIJ_1_OLD_IDX] = top_row[pe + 1];
    scoring[pe][HI_1J_OLD_IDX] = 0;
    degenerate[pe] = !(pe < query_len);
	}
	//// Compute the score
	int old_idx = 0;
	const int steps = subject_len + max(pe_count, query_len) - 1;
	InputStreamType olds[PE_COUNT];
#pragma HLS ARRAY_PARTITION variable=olds block=factor=224 dim=1
	systolic0: for(int i = 0; i < steps; i++) {
#pragma HLS PIPELINE II=1
		if(i < subject_len) {
			InputStreamType data = sub.read(); //[que_sub_it++];
			inputs[0][IAI_IDX] = data.data >> 16;
			inputs[0][IHI_1J_IDX] = (ScoreType)(data.data & 0xFFFF);
			olds[i % PE_COUNT] = data;

		}
		else {
			inputs[0][IAI_IDX] = INV_CHAR;
			inputs[0][IHI_1J_IDX] = 0;
		}

		systolic1: for(int pe = pe_count-1; pe >= 0; pe--) {
#pragma HLS LOOP_FLATTEN
      if(degenerate[pe]) {
        scoring[pe][HIJ_1_OLD_IDX] = inputs[pe][IHI_1J_IDX];
        if(pe + 1 < pe_count) {
          inputs[pe + 1][IHI_1J_IDX] = scoring[pe][HIJ_1_OLD_IDX];
        }
        continue;
      }
			const ScoreType iai = inputs[pe][IAI_IDX];

			ScoreType m1 = 0;
			if(pe < query_len && iai < INV_CHAR) {
				m1 = scoring[pe][HI_1J_1_OLD_IDX] + (iai == query_cpy[pe] ? MATCH : MISMATCH);
			}
			// relax
			const int ihi_1jbuf = inputs[pe][IHI_1J_IDX];
			const int hij_1_old = scoring[pe][HIJ_1_OLD_IDX];
			const ScoreType m2 = GAPPENALTY + max(hij_1_old, ihi_1jbuf);
			const ScoreType hij = iai == INV_CHAR ? ihi_1jbuf : max(m1, m2);

			// update my data
      if(iai < INV_CHAR) {
        scoring[pe][HI_1J_1_OLD_IDX] = ihi_1jbuf;
        scoring[pe][HIJ_1_OLD_IDX] = hij;
      }
      else {
        scoring[pe][HIJ_1_OLD_IDX] = hij_1_old;
      }
			// set input
			if(pe + 1 < pe_count)
			{
				inputs[pe + 1][IAI_IDX] = iai;
				inputs[pe + 1][IHI_1J_IDX] = hij;
			}
		}
		if(i >= pe_count - 1) {
			// write stuff to buffer
			//wbuff[i - pe_count + 1] = (scoring[pe_count - 1][HIJ_1_OLD_IDX]);
			OutputStreamType store;
			InputStreamType oldstor = olds[old_idx];
			old_idx = (old_idx >= pe_count - 1 ? 0 : old_idx + 1);

			store.data = (ScoreType)(scoring[pe_count - 1][HIJ_1_OLD_IDX] & 0xFFFF);
			store.last = (old_idx == 0 || i + 1 >= steps);

			store.dest = oldstor.dest;
			store.id = oldstor.id;
			store.keep = oldstor.keep;
			store.strb = oldstor.strb;
			store.user = oldstor.user;

			wbuff.write(store);
		}
	}
	*res = scoring[pe_count - 1][HIJ_1_OLD_IDX];
}


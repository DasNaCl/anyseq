
#include "align.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>

const int query_len = 717;
const int subject_len = 1031;

// Unfortunately, this *must* be the same cost as specified in export.impala
//  Otherwise, the test will fail
#define MATCH       1
#define MISMATCH   -1
#define GAPPENALTY -1

#ifndef max
#define max(a, b) (((a) < (b)) ? (b) : (a))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

int w(char a, char b)
{ return a == b ? MATCH : MISMATCH; }

int software_model(const std::string& subject, const std::string& query)
{
	int h[subject.size() + 1][query.size() + 1];

	// init
	h[0][0] = 0;
	for(int i = 0; i <= subject.size(); ++i)
		h[i][0] = 0;
	for(int j = 0; j <= query.size(); ++j)
		h[0][j] = 0;

	// solve recurrences
	int maximum = 0;
	for(int i = 1; i <= subject.size(); ++i) {
		for(int j = 1; j <= query.size(); ++j) {
			const int diag = w(subject[i - 1], query[j - 1]) + h[i - 1][j - 1];
			const int gap = GAPPENALTY + max(h[i - 1][j], h[i][j - 1]);

			h[i][j] = max(0, max(diag, gap));

			maximum = max(maximum, h[i][j]);
		}
	}
	return maximum;
}

char rand_base()
{
	switch(rand() % 4)
	{
	case 0: return 'A';
	case 1: return 'C';
	case 2: return 'G';
	case 3: return 'T';
	}
	return 42;
}

int test();

int main()
{
	srand(1234561);
	const int test_num = 16;

	std::cout << "********** START TB **********\n";

	int passed = 0;
	for(int i = 0; i < test_num; i++)
		passed += test();

	if(passed == test_num) {
		std::cout << "\n********** OK **********\n";
		return 0;
	}
	else {
		std::cout << "\n\n[][][][][][] FAIL [][][][][][]\n";
		return -1;
	}
}

int test()
{
	std::string query, conv_query;
	query.resize(query_len);
	conv_query.resize(query_len);

	std::string subject, conv_subject;
	subject.resize(subject_len);
	conv_subject.resize(subject_len);

	// init
	for(int i = 0; i < query.size(); i++) {
		query[i] = rand_base();
		switch(query[i])
		{
		case 'A': conv_query[i] = 0; break;
		case 'C': conv_query[i] = 1; break;
		case 'G': conv_query[i] = 2; break;
		case 'T': conv_query[i] = 3; break;
		}
	}
	for(int i = 0; i < subject.size(); i++) {
		subject[i] = rand_base();
		switch(subject[i])
		{
		case 'A': conv_subject[i] = 0; break;
		case 'C': conv_subject[i] = 1; break;
		case 'G': conv_subject[i] = 2; break;
		case 'T': conv_subject[i] = 3; break;
		}
	}

	// run the simulation
	std::stringstream fail_prefix_stream;
	fail_prefix_stream << "Aligning sequences \"" << query << "\" and \"" << subject << "\": ";

	/****
	 * HARDWARE
	 */
	// hardware only takes up to pe_count long queries, so we have to split that thing if it is longer
	short hw = 0;
	int offset = 0;
	hls::stream<OutputStreamType> wbuffer("wbuffer");
	for(int i = 0; i < subject.size(); i++) {
		OutputStreamType out;
		out.keep = 1; out.user = 1;	out.strb = 1; out.dest = 0; out.id = 0;
		out.data = 0;
		wbuffer.write(out);
	}
	int s_len = subject.size();
	do {
		int q_len = min(query.size() - offset, pe_count);

		hls::stream<InputStreamType> sub("sub");
		for(int i = 0; i < subject.size(); i++) {
			InputStreamType in;
			in.data = (conv_subject[i] << 16) | wbuffer.read().data;
			sub.write(in);
		}
		ScoreType local_result = 0;
		align(sub, &conv_query[offset], s_len, q_len, wbuffer, &local_result);

		hw = max(hw, ((short)local_result));
	} while((offset += pe_count) < query.size());
	while(!wbuffer.empty())
		wbuffer.read();

	/****
	 * SOFTWARE
	 */
	short sw = software_model(subject, query);

	fail_prefix_stream << " HARDWARE [" << hw << "] vs. [" << sw << "] SOFTWARE\n";

	if(hw == sw) {
		return 1;
	}
	else {
		// only print stuff if things have failed
		std::string str = fail_prefix_stream.str();
		std::cout << str;
		return 0;
	}
}


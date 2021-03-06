#include "config.hpp"
#include "utils.hpp"
#include "file_reader.hpp"
#include "stair_bf.hpp"
#include "stair_cm.hpp"
#include "stair_cu.hpp"
#include "pbf.hpp"
#include "hokusai.hpp"
#include "adacm.hpp"
#include "common.hpp"

template<class Sketch>
void build_sketch(Sketch *sketch) {
	if (sketch->add_delta_implemented()) {
		for (int i = 1; i <= cfg.win_num; ++i) 
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[i] - elems[k].cnt[i-1] > 0)
					sketch->add(i, elems[k].e, elems[k].cnt[i] - elems[k].cnt[i-1]);
	} else {
		for (int i = 1; i <= cfg.win_num; ++i)
			for (elem_t e : win_data[i])
				sketch->add(i, e);
	}
}

vector<tuple<int, int, int, int> > stair_config(int memory, int k) {
	int sum = 0;
	for (int i = 0; i <= k; ++i) 
		sum += (1 << i) * (i == k ? 4 : 1);

	double unit = (double) memory / sum;
	vector<tuple<int, int, int, int> > ds_config = 
		{ make_tuple(unit, 1, 1, 1) };

	for (int i = 1; i <= k; ++i)
		ds_config.push_back(make_tuple(unit * (1 << i) * (i == k ? 4 : 1), 2, i == k ? 4 : 1, 1 << (i-1)));
	return ds_config;
}

stair_cu* build_scu(int memory, int level = 3) {
	return new stair_cu(stair_config(memory, level));
}

stair_bf* build_sbf(int memory, int level = 3) {
	return new stair_bf(stair_config(memory, level));
}

persistent_bf* build_pbf(int memory) {
	return new persistent_bf(cfg.win_num, memory, 2);
}

pbf1* build_pbf1(int memory) {
	return new pbf1(cfg.win_num, memory, 2);
}

pbf2* build_pbf2(int memory) {
	return new pbf2(cfg.win_num, memory, 2);
}

stair_cm* build_scm(int memory, int level = 3) {
	return new stair_cm(stair_config(memory, level));
}

item_aggregation_bf* build_iabf(int memory) {
	return new item_aggregation_bf(memory, 2, cfg.ds_win_num);
}

time_aggregation_bf* build_tabf(int memory, int lv_num) {
	return new time_aggregation_bf(memory, 2, lv_num);
}

ada_cm* build_adacm(int memory) {
	return new ada_cm(memory, 2);
}

item_aggregation_cm* build_iacm(int memory) {
	return new item_aggregation_cm(memory, 2, cfg.ds_win_num);
}

template<class Sketch>
void bf_test_fpr(Sketch *sketch, double *fpr) {	
	build_sketch(sketch);
	int start = cfg.win_num - cfg.ds_win_num + 1;
	for (int i = start; i <= cfg.win_num; ++i) {
		int fp = 0, tot = 0;
		for (int k = 0; k < elem_cnt; ++k) {
			if (elems[k].cnt[i] - elems[k].cnt[i-1] == 0) {
				tot++;
				if (sketch->query(i, elems[k].e)) fp++;
			} else {
				assert(sketch->query(i, elems[k].e));
			}
		}
		fpr[i - start + 1] = (double) fp / tot;
	}
}

template<class Sketch>
void bf_test_multi_fpr(Sketch *sketch, double *fpr) {	
	build_sketch(sketch);
	int *tot = new int[cfg.ds_win_num + 1];
	int *mask = new int[elem_cnt];
	int start = cfg.win_num - cfg.ds_win_num;

	for (int i = 1; i <= cfg.ds_win_num; ++i) fpr[i] = tot[i] = 0;
	for (int k = 0; k < elem_cnt; ++k) {
		mask[k] = 0;
		for (int i = 1; i <= cfg.ds_win_num; ++i)
			if (sketch->query(start + i, elems[k].e)) mask[k] |= 1 << i;
	}
	for (int l = 1; l <= cfg.ds_win_num; ++l) {
		double wt = 0; int _mask = 0;
		for (int r = l; r <= cfg.ds_win_num; ++r) {
			wt += 1.0 / (cfg.ds_win_num - r + 1); _mask += 1 << r;
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[start + r] == elems[k].cnt[start + l - 1]) {
					tot[r - l + 1]++;
					if (mask[k] & _mask) fpr[r - l + 1] += wt;
				}
		}
	}
	for (int i = 1; i <= cfg.ds_win_num; ++i) fpr[i] /= tot[i];

	delete[] mask;
	delete[] tot;
}

template<class Sketch>
void bf_test_stability(Sketch *sketch, double *fpr) {
	for (int i = 1; i <= cfg.win_num; ++i) {
		if (sketch->add_delta_implemented()) {
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[i] - elems[k].cnt[i-1] > 0)
					sketch->add(i, elems[k].e, elems[k].cnt[i]);
		} else {
			for (elem_t e : win_data[i]) sketch->add(i, e);
		}

		int fp = 0, tot = 0;
		for (int k = 0; k < elem_cnt; ++k) {
			if (elems[k].cnt[i] - elems[k].cnt[i-1] == 0) {
				tot++;
				if (sketch->query(i, elems[k].e)) fp++;
			} else {
				assert(sketch->query(i, elems[k].e));
			}
		}
		fpr[i] = (double) fp / tot;
	}
	fprintf(stderr, "%d/%d\n", sketch->memory(), cfg.memory);
}


template<class Sketch>
void cnt_test_are(Sketch *sketch, double *are) {
	build_sketch(sketch);
	fprintf(stderr, "Memory %d/%d\n", sketch->memory(), cfg.memory);
	int start = cfg.win_num - cfg.ds_win_num + 1;
	for (int i = start; i <= cfg.win_num; ++i) {
		int tot = 0; are[i - start + 1] = 0;	
		for (auto pr : win_set[i]) {
			int real = pr.second, ans = sketch->query(i, pr.first);
			are[i - start + 1] += 1.0 * fabs(real - ans) / real;
			tot++;
		}
		are[i - start + 1] /= tot;
	}
}

template<class Sketch>
void cnt_test_multi_are(Sketch *sketch, double *are) {	
	build_sketch(sketch);
	int **sum = new int*[elem_cnt];
	int *tot = new int[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	for (int k = 0; k < elem_cnt; ++k) {
		sum[k] = new int[cfg.ds_win_num + 1];
		sum[k][0] = 0;
		for (int i = 1; i <= cfg.ds_win_num; ++i)
			sum[k][i] = sum[k][i-1] + sketch->query(start + i, elems[k].e);
	}

	for (int i = 1; i <= cfg.ds_win_num; ++i) are[i] = tot[i] = 0;
	for (int l = 1; l <= cfg.ds_win_num; ++l) {
		double wt = 0;
		for (int r = l; r <= cfg.ds_win_num; ++r) {
			wt += 1.0 / (cfg.ds_win_num - r + 1);
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[start + r] > elems[k].cnt[start + l - 1]) {
					tot[r - l + 1]++;
					int real = elems[k].cnt[start + r] - elems[k].cnt[start + l - 1], ans = sum[k][r] - sum[k][l - 1];
					are[r - l + 1] += wt * fabs(real - ans) / real;
				}
		}
	}
	for (int i = 1; i <= cfg.win_num; ++i) are[i] /= tot[i];

	for (int k = 0; k < elem_cnt; ++k) delete[] sum[k];
	delete[] sum;
	delete[] tot;
}

template<class Sketch>
void cnt_test_multi_aae(Sketch *sketch, double *aae) {	
	build_sketch(sketch);
	int **sum = new int*[elem_cnt];
	int *tot = new int[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	for (int k = 0; k < elem_cnt; ++k) {
		sum[k] = new int[cfg.ds_win_num + 1];
		sum[k][0] = 0;
		for (int i = 1; i <= cfg.ds_win_num; ++i)
			sum[k][i] = sum[k][i-1] + sketch->query(start + i, elems[k].e);
	}

	for (int i = 1; i <= cfg.ds_win_num; ++i) aae[i] = tot[i] = 0;
	for (int l = 1; l <= cfg.ds_win_num; ++l) {
		double wt = 0;
		for (int r = l; r <= cfg.ds_win_num; ++r) {
			wt += 1.0 / (cfg.ds_win_num - r + 1);
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[start + r] > elems[k].cnt[start + l - 1]) {
					tot[r - l + 1]++;
					int real = elems[k].cnt[start + r] - elems[k].cnt[start + l - 1], ans = sum[k][r] - sum[k][l - 1];
					aae[r - l + 1] += wt * fabs(real - ans);
				}
		}
	}
	for (int i = 1; i <= cfg.win_num; ++i) aae[i] /= tot[i];

	for (int k = 0; k < elem_cnt; ++k) delete[] sum[k];
	delete[] sum;
	delete[] tot;
}

template<class Sketch>
void cnt_test_aae(Sketch *sketch, double *aae) {
	build_sketch(sketch);
	int start = cfg.win_num - cfg.ds_win_num + 1;
	for (int i = start; i <= cfg.win_num; ++i) {
		int tot = 0; aae[i - start + 1] = 0;	
		for (int k = 0; k < elem_cnt; ++k) {
			if (elems[k].cnt[i] - elems[k].cnt[i-1] > 0) {
				int real = elems[k].cnt[i] - elems[k].cnt[i-1], ans = sketch->query(i, elems[k].e);
				aae[i - start + 1] += fabs(real - ans);
				tot++;
			}
		}
		aae[i - start + 1] /= tot;
	}
}

template<class Sketch>
void cnt_test_stability(Sketch *sketch, double *are) {
	for (int i = 1; i <= cfg.win_num; ++i) {
		if (sketch->add_delta_implemented()) {
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[i] - elems[k].cnt[i-1] > 0)
					sketch->add(i, elems[k].e, elems[k].cnt[i] - elems[k].cnt[i-1]);
		} else {
			for (elem_t e : win_data[i]) sketch->add(i, e);
		}
		int tot = 0; are[i] = 0;	
		for (int k = 0; k < elem_cnt; ++k) {
			if (elems[k].cnt[i] - elems[k].cnt[i-1] > 0) {
				int real = elems[k].cnt[i] - elems[k].cnt[i-1], ans = sketch->query(i, elems[k].e);
				are[i] += 1.0 * fabs(real - ans) / real;
				tot++;
			}
		}
		are[i] /= tot;
	}
}

double weighted_score(double *score) {
	double ret = 0;
	for (int i = 1; i <= cfg.ds_win_num; ++i) {
		ret += score[i] / (cfg.ds_win_num - i + 1);
		//fprintf(stderr, "%.4f ", score[i]);
	}
	//fprintf(stderr, " %.4f\n", ret);
	return ret;
}

template<class Sketch>
double bf_test_wfpr(Sketch *s) {
	double* fpr = new double[cfg.ds_win_num + 1];
	bf_test_fpr(s, fpr);
	return weighted_score(fpr);
}

template<class Sketch>
double cnt_test_ware(Sketch *s) {
	double* are = new double[cfg.ds_win_num + 1];
	cnt_test_are(s, are);
	return weighted_score(are);
}

template<class Sketch>
double cnt_test_waae(Sketch *s) {
	double* aae = new double[cfg.ds_win_num + 1];
	cnt_test_aae(s, aae);
	return weighted_score(aae);
}

template<class Sketch>
double bf_test_win_num_wfpr(Sketch *sketch) {	
	double *fpr = new double[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	for (int i = 1; i <= cfg.win_num; ++i)
		for (elem_t e : win_data[i])
			sketch->add(i, e);
	for (int i = 1; i <= cfg.ds_win_num; ++i) {
		int fp = 0, tot = 0;
		for (elem_t e : elem_set)
			if (win_set[start + i].count(e) == 0) {
				++tot;
				if (sketch->query(start + i, e)) ++fp;
			}
		fpr[i] = 1.0 * fp / tot;
	}
	return weighted_score(fpr);
}

template<class Sketch>
double cnt_test_win_num_ware(Sketch *sketch) {
	double *are = new double[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	for (int i = 1; i <= cfg.win_num; ++i)
		for (elem_t e : win_data[i])
			sketch->add(i, e);
	for (int i = 1; i <= cfg.ds_win_num; ++i) {
		are[i] = 0;
		int tot = 0;
		for (auto pr : win_set[start + i]) {
			int real = pr.second, ans = sketch->query(start + i, pr.first);
			are[i] += 1.0 * fabs(real - ans) / real;
			++tot;
		}
		are[i] /= tot;
	}
	return weighted_score(are);
}

template<class Sketch>
double cnt_test_win_num_waae(Sketch *sketch) {
	double *aae = new double[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	for (int i = 1; i <= cfg.win_num; ++i)
		for (elem_t e : win_data[i])
			sketch->add(i, e);
	for (int i = 1; i <= cfg.ds_win_num; ++i) {
		aae[i] = 0;
		int tot = 0;
		for (auto pr : win_set[start + i]) {
			int real = pr.second, ans = sketch->query(start + i, pr.first);
			aae[i] += 1.0 * fabs(real - ans);
			++tot;
		}
		aae[i] /= tot;
	}
	return weighted_score(aae);
}

template<class Sketch>
void bf_test_qcnt(Sketch *sketch, double *aqcnt) {
	build_sketch(sketch);
	int start = cfg.win_num - cfg.ds_win_num;
	int *tot = new int[cfg.ds_win_num + 1];
	long long *cnt = new long long[cfg.ds_win_num + 1];
	for (int i = 1; i <= cfg.ds_win_num; ++i) cnt[i] = tot[i] = 0;
	long long last = 0;

	for (int l = 1; l <= cfg.ds_win_num; ++l) {
		for (int r = l; r <= cfg.ds_win_num; ++r) {
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[start + r] == elems[k].cnt[start + l - 1]) {
					tot[r - l + 1]++;
					long long last = sketch->qcnt();
					sketch->query_multiple_windows(l, r, elems[k].e);
					cnt[r - l + 1] += sketch->qcnt() - last;
				}
		}
	}
	for (int i = 1; i <= cfg.ds_win_num; ++i)
		aqcnt[i] = 1.0 * cnt[i] / tot[i];

	delete[] cnt;
	delete[] tot;
}

template<class Sketch>
void cnt_test_qcnt(Sketch *sketch, double *aqcnt) {	
	build_sketch(sketch);
	int *tot = new int[cfg.ds_win_num + 1];
	int start = cfg.win_num - cfg.ds_win_num;
	long long *cnt = new long long[cfg.ds_win_num + 1];
	for (int i = 1; i <= cfg.ds_win_num; ++i) cnt[i] = tot[i] = 0;
	long long last = 0;

	for (int l = 1; l <= cfg.ds_win_num; ++l) {
		for (int r = l; r <= cfg.ds_win_num; ++r) {
			for (int k = 0; k < elem_cnt; ++k)
				if (elems[k].cnt[start + r] > elems[k].cnt[start + l - 1]) {
					tot[r - l + 1]++;
					long long last = sketch->qcnt();
					sketch->query_multiple_windows(l, r, elems[k].e);
					cnt[r - l + 1] += sketch->qcnt() - last;
				}
		}
	}
	for (int i = 1; i <= cfg.ds_win_num; ++i)
		aqcnt[i] = 1.0 * cnt[i] / tot[i];

	delete[] cnt;
	delete[] tot;
}
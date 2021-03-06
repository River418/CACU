/*
 Copyright (c) 2016, David lu
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the <organization> nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

namespace mycnn {

class batch_normalization_layer: public layer {

public:

	batch_normalization_layer(char_t layer_name, int input_dim, int channel,
			type phrase, float_t lr_w = 1.0, float_t lr_b = 1.0) :
			layer(layer_name, input_dim, channel, phrase, lr_w, lr_b) {
		this->layer_name = layer_name;
		this->stride = 1;
		this->output_dim = input_dim;
		this->pad = 0;
		this->channel = channel;
		this->input_dim = input_dim;
		this->output_channel = channel;
		this->phrase = phrase;
		this->set_lr_w(lr_w);
		this->set_lr_b(lr_b);
		INIT_SPACE_DATA();
		INIT_PARAM_SAPCE_DATA();
		INIT_STORAGE_DATA();
	}

#if GPU_MODE

	virtual const void forward() override
	{

		CACU_RESET_DATA_GPU(mean->data,1,channel);
		CACU_RESET_DATA_GPU(var->data,1,channel);
		CACU_RESET_DATA_GPU(std->data,1,channel);

		float_t m = (float_t)BATCH_SIZE*input_dim*input_dim;
		float_t bias_correction_factor = m > (float_t)1.0 ? (m) / (m - (float_t)1.0) : (float_t)1.0;

		if (train == phrase) {

			//update mean variance to moving
			CACU_MEAN_CHANNEL_GPU(bottoms[0]->data, bottoms[0]->num, input_dim*input_dim*channel,
					channel, storage_data->s_data["mean"]);

			//variance
			CACU_VARIANCE_CHANNEL_GPU(bottoms[0]->data, storage_data->s_data["mean"],
					bottoms[0]->num, input_dim*input_dim*channel, channel, storage_data->s_data["var"]);

			//unbiased estimate
			CACU_SCALE_GPU_A(storage_data->data["var"], (float_t(1) - moving_average_fraction)*bias_correction_factor, 1,channel, var->data, 0);

			CACU_SCALE_GPU_A(storage_data->data["smean"], moving_average_fraction, 1,channel , storage_data->data["smean"], 0);
			CACU_SCALE_GPU_A(storage_data->data["mean"], (float_t(1) - moving_average_fraction),1, channel, mean->data, 0);
			CACU_SUM_GPU_D(storage_data->data["smean"], mean->data,1,channel, storage_data->data["smean"]);

			CACU_SCALE_GPU_A(storage_data->data["svar"], moving_average_fraction,1, channel, storage_data->data["svar"], 0);
			CACU_SUM_GPU_D(storage_data->data["svar"], var->data,1,channel, storage_data->data["svar"]);

			//caculate std
			CACU_SUM_GPU_D(storage_data->data["var"], _epsilon->data,1, channel,std->data);
			CACU_SQRT_GPU(std->data,1,channel, std->data);

			CACU_SUB_GPU(bottoms[0]->data, storage_data->s_data["mean"], bottoms[0]->num,
					input_dim*input_dim*channel, channel, tops[0]->data);

			CACU_DIVISION_GPU(tops[0]->data,std->s_data, tops[0]->num,
					output_dim*output_dim*channel, channel, tops[0]->data);

			CACU_SCALE_GPU(tops[0]->data, params->s_data["scale"], tops[0]->num,output_dim*output_dim*channel, channel, tops[0]->data);

			CACU_SUM_GPU(tops[0]->data, params->s_data["shift"], tops[0]->num,output_dim*output_dim*channel,channel, tops[0]->data);

		}
		else {
			if (use_global_stats) {
				//caculate std
				CACU_SUM_GPU_D(storage_data->data["svar"], _epsilon->data,1, channel,std->data);
				CACU_SQRT_GPU(std->data,1,channel, std->data);

				CACU_SUB_GPU(bottoms[0]->data, storage_data->s_data["smean"], bottoms[0]->num,
						input_dim*input_dim*channel, channel, tops[0]->data);

				CACU_DIVISION_GPU(tops[0]->data,std->s_data, tops[0]->num,
						output_dim*output_dim*channel, channel, tops[0]->data);

				CACU_SCALE_GPU(tops[0]->data, params->s_data["scale"], tops[0]->num,output_dim*output_dim*channel, channel, tops[0]->data);

				CACU_SUM_GPU(tops[0]->data, params->s_data["shift"], tops[0]->num,output_dim*output_dim*channel ,channel, tops[0]->data);
			}
			else
			{
				if (test == phrase) {

					CACU_MEAN_CHANNEL_GPU(bottoms[0]->data, bottoms[0]->num, input_dim*input_dim*channel,
							channel, storage_data->s_data["mean"]);

					//variance
					CACU_VARIANCE_CHANNEL_GPU(bottoms[0]->data, storage_data->s_data["mean"],
							bottoms[0]->num, input_dim*input_dim*channel, channel, storage_data->s_data["var"]);
				}
				//caculate std
				CACU_SUM_GPU_D(storage_data->data["var"], _epsilon->data,1, channel,std->data);
				CACU_SQRT_GPU(std->data,1,channel, std->data);

				CACU_SUB_GPU(bottoms[0]->data, storage_data->s_data["mean"], bottoms[0]->num,
						input_dim*input_dim*channel, channel, tops[0]->data);

				CACU_DIVISION_GPU(tops[0]->data,std->s_data, tops[0]->num,
						output_dim*output_dim*channel, channel, tops[0]->data);

				CACU_SCALE_GPU(tops[0]->data, params->s_data["scale"], tops[0]->num,output_dim*output_dim*channel, channel, tops[0]->data);

				CACU_SUM_GPU(tops[0]->data, params->s_data["shift"], tops[0]->num,output_dim*output_dim*channel , channel, tops[0]->data);
			}
		}

//		printf("%s: ",layer_name.c_str());
//		cudaError_t res;
//		vec_t test_data(1);
//		res = cudaMemcpy((void*) (&test_data[0]), (void*) (storage_data->s_data["mean"]),
//				test_data.size() * sizeof(float_t), cudaMemcpyDeviceToHost);
//		CHECK(res);
//
//		printf("%s:%f,","mean",test_data[0]);
//		res = cudaMemcpy((void*) (&test_data[0]), (void*) (storage_data->s_data["var"]),
//				test_data.size() * sizeof(float_t), cudaMemcpyDeviceToHost);
//		CHECK(res);
//		printf("%s:%f,","var",test_data[0]);
//
//		res = cudaMemcpy((void*) (&test_data[0]), (void*) (storage_data->s_data["smean"]),
//				test_data.size() * sizeof(float_t), cudaMemcpyDeviceToHost);
//		CHECK(res);
//		printf("%s:%f,","smean",test_data[0]);
//
//		res = cudaMemcpy((void*) (&test_data[0]), (void*) (storage_data->s_data["svar"]),
//				test_data.size() * sizeof(float_t), cudaMemcpyDeviceToHost);
//		CHECK(res);
//		printf("%s:%f,","svar",test_data[0]);
//		printf("\n");
	}

	virtual const void backward(layer_param *&v) override
	{

		CACU_RESET_DATA_GPU(dx_ba->data,bottoms[0]->num,channel*input_dim*input_dim);
		CACU_RESET_DATA_GPU(d_std->data,1,channel);
		CACU_RESET_DATA_GPU(d_mu->data,1,channel);
		//caculate std
		CACU_SUM_GPU_D(storage_data->data["var"], _epsilon->data,1, channel,std->data);
		CACU_SQRT_GPU(std->data,1,channel, std->data);

		//calculate dl/x_
		CACU_SCALE_GPU(tops[0]->diff, params->s_data["scale"],tops[0]->num,input_dim*input_dim*channel, channel, dx_ba->data);

		//calculate dl/std^2
		CACU_ROU_GPU(bottoms[0]->data, dx_ba->data, storage_data->s_data["mean"], std->s_data, tops[0]->num,input_dim*input_dim*channel,channel, d_std->s_data);

		//calculate dl/mu
		CACU_MU_GPU(bottoms[0]->data, dx_ba->data, storage_data->s_data["mean"], std->s_data, d_std->s_data,tops[0]->num, input_dim*input_dim*channel ,channel, d_mu->s_data);

		//calculate dl/x
		CACU_DX_GPU(bottoms[0]->data, dx_ba->data, storage_data->s_data["mean"], std->s_data, d_std->s_data, d_mu->s_data, tops[0]->num,input_dim*input_dim*channel,channel, bottoms[0]->diff);

		//calculate dl/scale
		CACU_SCALE_GPU_B(tops[0]->diff, tops[0]->data, tops[0]->num,input_dim*input_dim*channel, channel, v->s_data["scale"]);
		//calculate dl/shift
		CACU_SUM_GPU_B(tops[0]->diff,tops[0]->num,input_dim*input_dim*channel, channel,v->s_data["shift"]);

//		cudaError_t res;
//		vec_t test_data(32);
//		res = cudaMemcpy((void*) (&test_data[0]), (void*) (bottoms[0]->s_diff),
//				test_data.size() * sizeof(float_t), cudaMemcpyDeviceToHost);
//		CHECK(res);
//		for(int i = 0; i < test_data.size(); i ++)
//		printf("%d:%f,",i,test_data[i]);
//		printf("\n");
	}

	virtual const void save(std::ostream& os) override {

	}

	virtual const void load(std::ifstream& is) override {

	}

	virtual const void setup() override {
		set_data_gpu(storage_data->data["smean"], 1, channel,
				0.0);
		set_data_gpu(storage_data->data["svar"], 1, channel,
				1.0);

		mean = new blob(1, channel, 1);
		var = new blob(1, channel, 1);
		std = new blob(1,channel,1);
		_epsilon = new blob(1,channel,1,epsilon);

		if(train == this->phrase) {
			dx_ba = new blob(bottoms[0]->num, channel, input_dim);
			d_std = new blob(1, channel, 1);
			d_mu = new blob(1, channel, 1);
		}
	}

	virtual const int caculate_data_space() override {

		assert(bottoms[0]->channel == channel);
		assert(bottoms[0]->dim == input_dim);

		int sum = 0;
		for (int i = 0; i < tops.size(); i++) {
			if (tops[i]->num > 0) {
				sum += (tops[i]->num*input_dim*input_dim*channel);
				if (train == phrase)
				sum += (tops[i]->num*input_dim*input_dim*channel);
			}
		}
		for (int i = 0; i < bin_tops.size(); i++) {
			if (bin_tops[i]->num > 0) {
				sum += (bin_tops[i]->num*input_dim*input_dim*channel / BIN_SIZE);
				if (train == phrase)
				sum += (bin_tops[i]->num*input_dim*input_dim*channel);
			}
		}

		printf("%s top costs : %d \n", layer_name.c_str(), sum);

		sum += params->caculate_space();
		sum += storage_data->caculate_space();
		printf("%s params costs %d \n", layer_name.c_str(), params->caculate_space());
	}

#else

	virtual const void forward() override
	{

		CACU_RESET_CPU(mean->data);
		CACU_RESET_CPU(var->data);
		CACU_RESET_CPU(std->data);

		float_t m = (float_t)BATCH_SIZE*input_dim*input_dim;
		float_t bias_correction_factor = m > (float_t)1.0 ? (m) / (m - (float_t)1.0) : (float_t)1.0;

		if (train == phrase) {

			//update mean variance to moving
			CACU_MEAN_CHANNEL_CPU(bottoms[0]->data, channel, storage_data->data["mean"][0]);

			//variance
			CACU_VARIANCE_CHANNEL_CPU(bottoms[0]->data, storage_data->data["mean"][0], channel, storage_data->data["var"][0]);

			//unbiased estimate
			CACU_SCALE_CPU(storage_data->data["var"][0], (float_t(1) - moving_average_fraction)*bias_correction_factor, var->data[0], 0);

			CACU_SCALE_CPU(storage_data->data["smean"][0], moving_average_fraction, storage_data->data["smean"][0], 0);
			CACU_SCALE_CPU(storage_data->data["mean"][0], (float_t(1) - moving_average_fraction), mean->data[0], 0);
			CACU_SUM_CPU(storage_data->data["smean"], mean->data, storage_data->data["smean"]);

			CACU_SCALE_CPU(storage_data->data["svar"][0], moving_average_fraction, storage_data->data["svar"][0], 0);
			CACU_SUM_CPU(storage_data->data["svar"], var->data, storage_data->data["svar"]);

			//caculate std
			CACU_SUM_CPU(storage_data->data["var"], _epsilon->data, std->data);
			CACU_SQRT(std->data[0], std->data[0]);

			CACU_SUB_CPU(bottoms[0]->data, storage_data->data["mean"][0], channel, tops[0]->data);

			CACU_DIVISION_CPU(tops[0]->data, std->data[0], channel, tops[0]->data);

			CACU_SCALE_CPU(tops[0]->data, params->data["scale"][0], channel, tops[0]->data);

			CACU_SUM_CPU(tops[0]->data, params->data["shift"][0], channel, tops[0]->data);

		}
		else {
			if (use_global_stats) {
				//caculate std
				CACU_SUM_CPU(storage_data->data["svar"], _epsilon->data, std->data);
				CACU_SQRT(std->data[0], std->data[0]);

				CACU_SUB_CPU(bottoms[0]->data, storage_data->data["smean"][0], channel, tops[0]->data);

				CACU_DIVISION_CPU(tops[0]->data, std->data[0], channel, tops[0]->data);

				CACU_SCALE_CPU(tops[0]->data, params->data["scale"][0], channel, tops[0]->data);

				CACU_SUM_CPU(tops[0]->data, params->data["shift"][0], channel, tops[0]->data);
			}
			else
			{
				if (test == phrase) {

					CACU_MEAN_CHANNEL_CPU(bottoms[0]->data, channel, storage_data->data["mean"][0]);

					//variance
					CACU_VARIANCE_CHANNEL_CPU(bottoms[0]->data, storage_data->data["mean"][0], channel, storage_data->data["var"][0]);
				}
				//caculate std
				CACU_SUM_CPU(storage_data->data["var"], _epsilon->data, std->data);
				CACU_SQRT(std->data[0], std->data[0]);

				CACU_SUB_CPU(bottoms[0]->data, storage_data->data["mean"][0], channel, tops[0]->data);

				CACU_DIVISION_CPU(tops[0]->data, std->data[0], channel, tops[0]->data);

				CACU_SCALE_CPU(tops[0]->data, params->data["scale"][0], channel, tops[0]->data);

				CACU_SUM_CPU(tops[0]->data, params->data["shift"][0], channel, tops[0]->data);
			}
		}
		printf("bn: %s smean:%f,svar:%f,mean:%f,std:%f\n", layer_name.c_str(), storage_data->data["smean"][0][0], storage_data->data["svar"][0][0], storage_data->data["mean"][0][0], storage_data->data["var"][0][0]);

	}

	virtual const void backward(layer_param *&v) override
	{
		CACU_RESET_CPU(dx_ba->data);
		CACU_RESET_CPU(d_std->data);
		CACU_RESET_CPU(d_mu->data);

		CACU_RESET_CPU(std->data);

		//calculate std
		CACU_SUM_CPU(storage_data->data["var"], _epsilon->data, std->data);
		CACU_SQRT(std->data[0], std->data[0]);

		//calculate dl/x_
		CACU_SCALE_CPU(tops[0]->diff, params->data["scale"][0], channel, dx_ba->data);

		//calculate dl/std^2
		CACU_ROU_CPU(bottoms[0]->data, dx_ba->data, storage_data->data["mean"][0], std->data[0], channel, d_std->data[0]);
		//calculate dl/mu
		CACU_MU_CPU(bottoms[0]->data, dx_ba->data, storage_data->data["mean"][0], std->data[0], d_std->data[0], channel, d_mu->data[0]);

		//calculate dl/x
		CACU_DX_CPU(bottoms[0]->data, dx_ba->data, storage_data->data["mean"][0], std->data[0], d_std->data[0], d_mu->data[0], channel, bottoms[0]->diff);

		//calculate dl/scale
		CACU_SCALE_CPU(tops[0]->diff, tops[0]->data, channel, v->data["scale"][0]);
		//calculate dl/shift
		CACU_SUM_CPU(tops[0]->diff, channel, v->data["shift"][0]);

	}

	virtual const void save(std::ostream& os) override {
		os << layer_name;
		os << " scale:";
		for (auto scale : params->data["scale"]) {
			for (auto w : scale) os << w << ",";
		}
		os << " shift:";
		for (auto shift : params->data["shift"]) {
			for (auto w : shift) os << w << ",";
		}
		os << " smean:";
		for (auto smean : storage_data->data["smean"]) {
			for (auto w : smean) os << w << ",";
		}
		os << " svar:";
		for (auto svar : storage_data->data["svar"]) {
			for (auto w : svar) os << w << ",";
		}
		os << "\n";
	}

	virtual const void load(std::ifstream& is) override {

		string _p_layer;
		getline(is, _p_layer, '\n');

		vector<string> data;
		vector<string> pdata;

		data = split(_p_layer, " ");

		assert(data[0] == layer_name);

		int start;

		pdata = split(split(data[1], ":")[1], ",");
		for (int num = 0; num < params->data["scale"].size(); num++)
		{
			start = num * params->data["scale"][0].size();
			for (int k = 0; k < params->data["scale"][0].size(); k++)
			{
				params->data["scale"][num][k] = (float_t)atof(pdata[start + k].c_str());
			}
		}

		pdata = split(split(data[2], ":")[1], ",");
		for (int num = 0; num < params->data["shift"].size(); num++)
		{
			start = num * params->data["shift"][0].size();
			for (int k = 0; k < params->data["shift"][0].size(); k++)
			{
				params->data["shift"][num][k] = (float_t)atof(pdata[start + k].c_str());
			}
		}

		pdata = split(split(data[3], ":")[1], ",");
		for (int num = 0; num < storage_data->data["smean"].size(); num++)
		{
			start = num * storage_data->data["smean"][0].size();
			for (int k = 0; k < storage_data->data["smean"][0].size(); k++)
			{
				storage_data->data["smean"][num][k] = (float_t)atof(pdata[start + k].c_str());
			}
		}

		pdata = split(split(data[4], ":")[1], ",");
		for (int num = 0; num < storage_data->data["svar"].size(); num++)
		{
			start = num * storage_data->data["svar"][0].size();
			for (int k = 0; k < storage_data->data["svar"][0].size(); k++)
			{
				storage_data->data["svar"][num][k] = (float_t)atof(pdata[start + k].c_str());
			}
		}
		vector<string>().swap(data);
		vector<string>().swap(pdata);

	}

	virtual const void setup() override {

		vector<vec_t> zeros(1, vec_t(channel, 0.0));
		vector<vec_t> ones(1, vec_t(channel, 1.0));

		copy_data(zeros, storage_data->data["smean"], 0);
		copy_data(ones, storage_data->data["svar"], 0);

		mean = new blob(1, channel, 1);
		var = new blob(1, channel, 1);
		std = new blob(1,channel,1);
		_epsilon = new blob(1,channel,1,epsilon);

		if(train == this->phrase) {
			dx_ba = new blob(bottoms[0]->num, channel, input_dim);
			d_std = new blob(1, channel, 1);
			d_mu = new blob(1, channel, 1);
		}
	}

	virtual const int caculate_data_space() override {

		assert(bottoms[0]->data[0].size() == channel*input_dim*input_dim);

		int sum = 0;
		for (int i = 0; i < tops.size(); i++) {
			if (tops[i]->data.size() > 0) {
				sum += (tops[i]->data.size()*tops[i]->data[0].size());
				if (train == phrase)
				sum += tops[i]->diff.size()*tops[i]->diff[0].size();
			}
		}
		for (int i = 0; i < bin_tops.size(); i++) {
			if (bin_tops[i]->bin_data.size() > 0) {
				sum += (bin_tops[i]->bin_data.size()*bin_tops[i]->bin_data[0].size() / BIN_SIZE);
				if (train == phrase)
				sum += bin_tops[i]->diff.size()*bin_tops[i]->diff[0].size();
			}
		}

		printf("%s top costs : %d \n", layer_name.c_str(), sum);

		sum += params->caculate_space();
		sum += storage_data->caculate_space();
		printf("%s params costs %d \n", layer_name.c_str(), params->caculate_space());

		return sum;
	}

#endif

	virtual const void INIT_PARAM_SAPCE_DATA() override {
		//param_dim equals to _param_dim
		map<char_t, int> _param_outnum;
		map<char_t, int> _param_dim;

		map<char_t, int> _bin_param_outnum;
		map<char_t, int> _bin_param_dim;

		//here to initial the layer's params size
		////////////////////////////////////////

		_param_outnum["scale"] = 1;
		_param_dim["scale"] = channel;

		_param_outnum["shift"] = 1;
		_param_dim["shift"] = channel;

		////////////////////////////////////////

		_pPARAMS.push_back(_param_outnum);
		_pPARAMS.push_back(_param_dim);
		_pPARAMS.push_back(_bin_param_outnum);
		_pPARAMS.push_back(_bin_param_dim);
	}

	virtual const void INIT_SPACE_DATA() override {

		//param_dim equals to channel * dim * dim
		vector<int> _param_outnum;
		vector<int> _param_dim;

		vector<int> _bin_param_outnum;
		vector<int> _bin_param_dim;

		//here to initial the layer's top space size
		////////////////////////////////////////

		_param_outnum.push_back(channel);
		_param_dim.push_back(this->output_dim);

		////////////////////////////////////////

		for (int i = 0; i < _param_dim.size(); i++) {
			blob *top;
			tops.push_back(top);
		}

		for (int i = 0; i < _bin_param_dim.size(); i++) {
			bin_blob *bin_top;
			bin_tops.push_back(bin_top);
		}

		_PARAMS.push_back(_param_outnum);
		_PARAMS.push_back(_param_dim);
		_PARAMS.push_back(_bin_param_outnum);
		_PARAMS.push_back(_bin_param_dim);
	}

	virtual const void INIT_STORAGE_DATA() {
		map<char_t, int> _param_outnum;
		map<char_t, int> _param_dim;

		map<char_t, int> _bin_param_outnum;
		map<char_t, int> _bin_param_dim;

		//here to initial the layer's params size
		////////////////////////////////////////

		_param_outnum["smean"] = 1;
		_param_dim["smean"] = channel;

		_param_outnum["svar"] = 1;
		_param_dim["svar"] = channel;

		_param_outnum["mean"] = 1;
		_param_dim["mean"] = channel;

		_param_outnum["var"] = 1;
		_param_dim["var"] = channel;

		////////////////////////////////////////

		_pSTORAGE.push_back(_param_outnum);
		_pSTORAGE.push_back(_param_dim);
		_pSTORAGE.push_back(_bin_param_outnum);
		_pSTORAGE.push_back(_bin_param_dim);
	}

	~batch_normalization_layer() {

		DELETE_DATA();
	}

	bool use_global_stats = false;
	float_t moving_average_fraction = 0.9;

private:

	float_t epsilon = (float_t) 0.00001;

	blob *mean = NULL;
	blob *var = NULL;
	blob *std = NULL;
	blob *_epsilon = NULL;
	blob *dx_ba = NULL;
	blob *d_std = NULL;
	blob *d_mu = NULL;

	void DELETE_DATA() {
		delete mean;
		delete var;
		delete std;
		delete _epsilon;
		delete dx_ba;
		delete d_std;
		delete d_mu;

	}

};

}
;

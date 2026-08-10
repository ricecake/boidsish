#ifndef HELPERS_MLP_GLSL
#define HELPERS_MLP_GLSL

#define MLP_MAX_LAYERS 16
#define MLP_MAX_NEURONS 64

struct LayerInfo {
	int input_size;
	int output_size;
	int weight_offset;
	int bias_offset;
	int activation; // 0 = Identity, 1 = ReLU, 2 = LeakyReLU, 3 = Sigmoid, 4 = Tanh, 5 = Sine
};

struct MLPMetadata {
	int num_layers;
	int max_layer_size;
	LayerInfo layers[MLP_MAX_LAYERS];
};

layout(std430, binding = [[MLP_PARAMS_BINDING]]) readonly buffer MLPParamsSSBO {
	MLPMetadata u_mlp_metadata;
	float u_mlp_params[];
};

float mlp_activate(float x, int activation) {
	if (activation == 1) { // ReLU
		return max(0.0, x);
	} else if (activation == 2) { // LeakyReLU
		return x > 0.0 ? x : x * 0.2;
	} else if (activation == 3) { // Sigmoid
		return 1.0 / (1.0 + exp(-x));
	} else if (activation == 4) { // Tanh
		return tanh(x);
	} else if (activation == 5) { // Sine (SIREN Style)
		return sin(x);
	}
	return x; // Identity / None
}

// Evaluates the MLP with a given input array and writes to the output array.
void evaluate_mlp(in float input_val[MLP_MAX_NEURONS], out float output_val[MLP_MAX_NEURONS]) {
	float temp_buffers[2][MLP_MAX_NEURONS];

	int num_layers = u_mlp_metadata.num_layers;
	if (num_layers <= 0) return;

	int in_size = u_mlp_metadata.layers[0].input_size;
	for (int i = 0; i < in_size; ++i) {
		temp_buffers[0][i] = input_val[i];
	}

	int current_buf = 0;

	for (int l = 0; l < num_layers; ++l) {
		int n_in = u_mlp_metadata.layers[l].input_size;
		int n_out = u_mlp_metadata.layers[l].output_size;
		int w_off = u_mlp_metadata.layers[l].weight_offset;
		int b_off = u_mlp_metadata.layers[l].bias_offset;
		int act = u_mlp_metadata.layers[l].activation;

		int next_buf = 1 - current_buf;

		for (int j = 0; j < n_out; ++j) {
			float sum = u_mlp_params[b_off + j];
			for (int i = 0; i < n_in; ++i) {
				sum += temp_buffers[current_buf][i] * u_mlp_params[w_off + j * n_in + i];
			}
			temp_buffers[next_buf][j] = mlp_activate(sum, act);
		}

		current_buf = next_buf;
	}

	// Copy final output
	int out_size = u_mlp_metadata.layers[num_layers - 1].output_size;
	for (int i = 0; i < out_size; ++i) {
		output_val[i] = temp_buffers[current_buf][i];
	}
}

#endif // HELPERS_MLP_GLSL

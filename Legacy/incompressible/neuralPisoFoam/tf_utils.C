#include "tf_utils.H"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

namespace tf_utils {

// Tensorflow requirements
const char* TFDataTypeToString(TF_DataType data_type) {
  switch (data_type) {
  case TF_FLOAT:
    return "TF_FLOAT";
  case TF_DOUBLE:
    return "TF_DOUBLE";
  case TF_INT32:
    return "TF_INT32";
  case TF_UINT8:
    return "TF_UINT8";
  case TF_INT16:
    return "TF_INT16";
  case TF_INT8:
    return "TF_INT8";
  case TF_STRING:
    return "TF_STRING";
  case TF_COMPLEX64:
    return "TF_COMPLEX64";
  case TF_INT64:
    return "TF_INT64";
  case TF_BOOL:
    return "TF_BOOL";
  case TF_QINT8:
    return "TF_QINT8";
  case TF_QUINT8:
    return "TF_QUINT8";
  case TF_QINT32:
    return "TF_QINT32";
  case TF_BFLOAT16:
    return "TF_BFLOAT16";
  case TF_QINT16:
    return "TF_QINT16";
  case TF_QUINT16:
    return "TF_QUINT16";
  case TF_UINT16:
    return "TF_UINT16";
  case TF_COMPLEX128:
    return "TF_COMPLEX128";
  case TF_HALF:
    return "TF_HALF";
  case TF_RESOURCE:
    return "TF_RESOURCE";
  case TF_VARIANT:
    return "TF_VARIANT";
  case TF_UINT32:
    return "TF_UINT32";
  case TF_UINT64:
    return "TF_UINT64";
  default:
    return "Unknown";
  }
}

static void DeallocateBuffer(void* data, size_t) {
  std::free(data);
}

static TF_Buffer* ReadBufferFromFile(const char* file) {

  std::ifstream f(file, std::ios::binary);
  if (f.fail() || !f.is_open()) {
    return nullptr;
  }

  f.seekg(0, std::ios::end);
  const auto fsize = f.tellg();
  f.seekg(0, std::ios::beg);

  if (fsize < 1) {
    f.close();
    return nullptr;
  }

  char* data = static_cast<char*>(std::malloc(fsize));
  f.read(data, fsize);
  f.close();

  TF_Buffer* buf = TF_NewBuffer();
  buf->data = data;
  buf->length = fsize;
  buf->data_deallocator = DeallocateBuffer;

  return buf;
}

TF_Graph* LoadGraph(const char* graphPath) {
  if (graphPath == nullptr) {
    return nullptr;
  }

  TF_Buffer* buffer = ReadBufferFromFile(graphPath);
  if (buffer == nullptr) {
    return nullptr;
  }

  TF_Graph* graph = TF_NewGraph();
  TF_Status* status = TF_NewStatus();
  TF_ImportGraphDefOptions* opts = TF_NewImportGraphDefOptions();

  TF_GraphImportGraphDef(graph, buffer, opts, status);
  TF_DeleteImportGraphDefOptions(opts);
  TF_DeleteBuffer(buffer);

  if (TF_GetCode(status) != TF_OK) {
    TF_DeleteGraph(graph);
    graph = nullptr;
  }
  TF_DeleteStatus(status);

  return graph;
}

void DeleteGraph(TF_Graph* graph) {
  TF_DeleteGraph(graph);
}

void DeleteSession(TF_Session* session) {
  TF_Status* status = TF_NewStatus();
  TF_CloseSession(session, status);
  if (TF_GetCode(status) != TF_OK) {
    TF_CloseSession(session, status);
  }
  TF_DeleteSession(session, status);
  if (TF_GetCode(status) != TF_OK) {
    TF_DeleteSession(session, status);
  }
  TF_DeleteStatus(status);
}

TF_Session* CreateSession(TF_Graph* graph) {
  TF_Status* status = TF_NewStatus();
  TF_SessionOptions* options = TF_NewSessionOptions();
  TF_Session* session = TF_NewSession(graph, options, status);
  TF_DeleteSessionOptions(options);

  if (TF_GetCode(status) != TF_OK) {
    DeleteSession(session);
    TF_DeleteStatus(status);
    return nullptr;
  }
  TF_DeleteStatus(status);

  return session;
}

TF_Code RunSession(TF_Session* session,
                   const TF_Output* inputs, TF_Tensor* const* input_tensors, std::size_t ninputs,
                   const TF_Output* outputs, TF_Tensor** output_tensors, std::size_t noutputs) {
  if (session == nullptr ||
      inputs == nullptr || input_tensors == nullptr ||
      outputs == nullptr || output_tensors == nullptr) {
    return TF_INVALID_ARGUMENT;
  }

  TF_Status* status = TF_NewStatus();
  TF_SessionRun(session,
                nullptr, // Run options.
                inputs, input_tensors, static_cast<int>(ninputs), // Input tensors, input tensor values, number of inputs.
                outputs, output_tensors, static_cast<int>(noutputs), // Output tensors, output tensor values, number of outputs.
                nullptr, 0, // Target operations, number of targets.
                nullptr, // Run metadata.
                status // Output status.
  );

  TF_Code code = TF_GetCode(status);
  TF_DeleteStatus(status);
  return code;
}

TF_Code RunSession(TF_Session* session,
                   const std::vector<TF_Output>& inputs, const std::vector<TF_Tensor*>& input_tensors,
                   const std::vector<TF_Output>& outputs, std::vector<TF_Tensor*>& output_tensors) {
  return RunSession(session,
                    inputs.data(), input_tensors.data(), input_tensors.size(),
                    outputs.data(), output_tensors.data(), output_tensors.size());
}

TF_Tensor* CreateTensor(TF_DataType data_type,
                        const std::int64_t* dims, std::size_t num_dims,
                        const void* data, std::size_t len) {
  if (dims == nullptr) {
    return nullptr;
  }

  TF_Tensor* tensor = TF_AllocateTensor(data_type, dims, static_cast<int>(num_dims), len);
  if (tensor == nullptr) {
    return nullptr;
  }

  void* tensor_data = TF_TensorData(tensor);
  if (tensor_data == nullptr) {
    TF_DeleteTensor(tensor);
    return nullptr;
  }

  if (data != nullptr) {
    std::memcpy(tensor_data, data, std::min(len, TF_TensorByteSize(tensor)));
  }

  return tensor;
}

TF_Tensor* CreateEmptyTensor(TF_DataType data_type, const std::int64_t* dims, std::size_t num_dims){
  return CreateTensor(data_type, dims, num_dims, nullptr, 0);
}

TF_Tensor* CreateEmptyTensor(TF_DataType data_type, const std::vector<std::int64_t>& dims) {
  return CreateEmptyTensor(data_type, dims.data(), dims.size());
}

void DeleteTensor(TF_Tensor* tensor) {
  if (tensor != nullptr) {
    TF_DeleteTensor(tensor);
  }
}

void DeleteTensors(const std::vector<TF_Tensor*>& tensors) {
  for (auto t : tensors) {
    TF_DeleteTensor(t);
  }
}

void SetTensorsData(TF_Tensor* tensor, const void* data, std::size_t len) {
  void* tensor_data = TF_TensorData(tensor);
  if (tensor_data != nullptr) {
    std::memcpy(tensor_data, data, std::min(len, TF_TensorByteSize(tensor)));
  }
}

// Some operations using tf_utils
void PrintInputs(TF_Graph*, TF_Operation* op) {
  const int num_inputs = TF_OperationNumInputs(op);

  for (int i = 0; i < num_inputs; ++i) {
    const TF_Input input = {op, i};
    const TF_DataType type = TF_OperationInputType(input);
    std::cout << "Input: " << i << " type: " << TFDataTypeToString(type) << std::endl;
  }
}

void PrintOutputs(TF_Graph* graph, TF_Operation* op) {
  const int num_outputs = TF_OperationNumOutputs(op);
  TF_Status* status = TF_NewStatus();

  for (int i = 0; i < num_outputs; ++i) {
    const TF_Output output = {op, i};
    const TF_DataType type = TF_OperationOutputType(output);
    const int num_dims = TF_GraphGetTensorNumDims(graph, output, status);

    if (TF_GetCode(status) != TF_OK) {
      std::cout << "Can't get tensor dimensionality" << std::endl;
      continue;
    }

    std::cout << " dims: " << num_dims;

    if (num_dims <= 0) {
      std::cout << " []" << std::endl;;
      continue;
    }

    std::vector<std::int64_t> dims(num_dims);

    std::cout << "Output: " << i << " type: " << TFDataTypeToString(type);
    TF_GraphGetTensorShape(graph, output, dims.data(), num_dims, status);

    if (TF_GetCode(status) != TF_OK) {
      std::cout << "Can't get get tensor shape" << std::endl;
      continue;
    }

    std::cout << " [";
    for (int d = 0; d < num_dims; ++d) {
      std::cout << dims[d];
      if (d < num_dims - 1) {
        std::cout << ", ";
      }
    }
    std::cout << "]" << std::endl;
  }

  TF_DeleteStatus(status);
}

void PrintTensorInfo(TF_Graph* graph, const char* layer_name) {
  std::cout << "Tensor: " << layer_name;
  TF_Operation* op = TF_GraphOperationByName(graph, layer_name);

  if (op == nullptr) {
    std::cout << "Could not get " << layer_name << std::endl;
    return;
  }

  const int num_inputs = TF_OperationNumInputs(op);
  const int num_outputs = TF_OperationNumOutputs(op);
  std::cout << " inputs: " << num_inputs << " outputs: " << num_outputs << std::endl;

  PrintInputs(graph, op);

  PrintOutputs(graph, op);
}

void PrintOp(TF_Graph* graph) {
  TF_Operation* op;
  std::size_t pos = 0;

  while ((op = TF_GraphNextOperation(graph, &pos)) != nullptr) {
    const char* name = TF_OperationName(op);
    const char* type = TF_OperationOpType(op);
    const char* device = TF_OperationDevice(op);

    const int num_outputs = TF_OperationNumOutputs(op);
    const int num_inputs = TF_OperationNumInputs(op);

    std::cout << pos << ": " << name << " type: " << type << " device: " << device << " number inputs: " << num_inputs << " number outputs: " << num_outputs << std::endl;

    PrintInputs(graph, op);
    PrintOutputs(graph, op);
    std::cout << std::endl;
  }
}

} // namespace tf_utils

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

#pragma once
// Optional non-blocking timing of the custom command list, not the whole game's GPU work.
struct GpuTiming {
 ID3D12QueryHeap* heap=nullptr;ID3D12Resource* results=nullptr;uint64_t frequency=0,*mapped=nullptr;bool pending[3]={};
 double sum=0,peak=0;unsigned samples=0;
 void init(ID3D12Device*d,ID3D12CommandQueue*q){
  if(FAILED(q->GetTimestampFrequency(&frequency))||!frequency)return;
  D3D12_QUERY_HEAP_DESC desc={};desc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;desc.Count=6;
  if(FAILED(d->CreateQueryHeap(&desc,__uuidof(ID3D12QueryHeap),(void**)&heap)))return;
  D3D12_HEAP_PROPERTIES hp={};hp.Type=D3D12_HEAP_TYPE_READBACK;hp.CreationNodeMask=hp.VisibleNodeMask=1;
  D3D12_RESOURCE_DESC rd={};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=256;rd.Height=1;rd.DepthOrArraySize=rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if(FAILED(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&results))){heap->Release();heap=nullptr;return;}
  D3D12_RANGE read={0,48};if(FAILED(results->Map(0,&read,(void**)&mapped))){results->Release();heap->Release();heap=nullptr;results=nullptr;}
 }
 void begin(ID3D12GraphicsCommandList*l,unsigned slot){if(!mapped)return;
  // Caller has already confirmed this ring slot's fence completed.
  if(pending[slot]){auto a=mapped[slot*2],b=mapped[slot*2+1];pending[slot]=false;if(b>=a){double ms=1000.*double(b-a)/double(frequency);if(ms>=0&&ms<1000){sum+=ms;peak=fmax(peak,ms);samples++;if(samples>=120){log("RENDER_GPU custom_avg_ms=%.2f custom_peak_ms=%.2f samples=%u",sum/samples,peak,samples);sum=peak=0;samples=0;}}}}
  l->EndQuery(heap,D3D12_QUERY_TYPE_TIMESTAMP,slot*2);
 }
 void end(ID3D12GraphicsCommandList*l,unsigned slot){if(!mapped)return;l->EndQuery(heap,D3D12_QUERY_TYPE_TIMESTAMP,slot*2+1);l->ResolveQueryData(heap,D3D12_QUERY_TYPE_TIMESTAMP,slot*2,2,results,slot*16);pending[slot]=true;}
};

%[text] 将输入参数typecast序列化为字节向量，用于写入流
%[text] ## 语法
%[text] ```matlabCodeExample
%[text] Bytes=Async_stream_IO.ArgumentSerialize(…);
%[text] ```
%[text] ## 输入参数
%[text] …，任意多个支持typecast到uint8的类型的参数
%[text] ## 返回值
%[text] Bytes(:,1)uint8，字节向量
function Bytes=ArgumentSerialize(varargin)
for V=1:nargin
	varargin{V}=typecast(varargin{V}(:),'uint8');
	%typecast总有可能返回行向量（例如输入是标量），必须全部统一成列向量
	varargin{V}=varargin{V}(:);
end
Bytes=vertcat(varargin{:});
end

%[appendix]{"version":"1.0"}
%---

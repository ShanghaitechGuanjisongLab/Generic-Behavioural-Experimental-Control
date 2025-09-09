function V=Version
V.Me='v7.0.0';
V.MatlabExtension='18.1.0';
V.MATLAB='R2025a';
V.Deploy=10;
persistent NewVersion
if isempty(NewVersion)
	NewVersion=TextAnalytics.CheckUpdateFromGitHub('https://github.com/ShanghaitechGuanjisongLab/Generic-Behavioural-Experimental-Control/releases','通用行为实验控制',V.Me);
end
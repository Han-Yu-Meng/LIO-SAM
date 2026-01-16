#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Git Diff 导出工具
自动读取 Git 历史，选择提交范围，生成供 AI 阅读的 diff 文件
支持屏蔽指定目录
"""

import subprocess
import sys
import os
from datetime import datetime
from pathlib import Path


class GitDiffExporter:
    def __init__(self, repo_path="."):
        self.repo_path = Path(repo_path).resolve()
        self.excluded_dirs = []
        
    def check_git_repo(self):
        """检查是否在 Git 仓库中"""
        try:
            subprocess.run(
                ["git", "rev-parse", "--git-dir"],
                cwd=self.repo_path,
                check=True,
                capture_output=True
            )
            return True
        except subprocess.CalledProcessError:
            print("❌ 错误: 当前目录不是一个 Git 仓库")
            return False
    
    def get_commit_log(self, limit=20):
        """获取最近的提交历史"""
        cmd = [
            "git", "log",
            f"--max-count={limit}",
            "--pretty=format:%h|%an|%ar|%s",
            "--date=relative"
        ]
        
        result = subprocess.run(
            cmd,
            cwd=self.repo_path,
            capture_output=True,
            text=True
        )
        
        commits = []
        for line in result.stdout.strip().split('\n'):
            if line:
                hash_id, author, date, message = line.split('|', 3)
                commits.append({
                    'hash': hash_id,
                    'author': author,
                    'date': date,
                    'message': message
                })
        
        return commits
    
    def display_commits(self, commits):
        """显示提交列表"""
        print("\n" + "=" * 80)
        print("最近的提交记录:")
        print("=" * 80)
        
        for idx, commit in enumerate(commits, 1):
            print(f"{idx:3d}. [{commit['hash']}] {commit['message']}")
            print(f"     作者: {commit['author']} | {commit['date']}")
            print()
    
    def get_user_input(self, prompt, default=None):
        """获取用户输入"""
        if default:
            prompt = f"{prompt} [{default}]: "
        else:
            prompt = f"{prompt}: "
        
        value = input(prompt).strip()
        return value if value else default
    
    def select_commit_range(self):
        """交互式选择提交范围"""
        commits = self.get_commit_log(limit=30)
        
        if not commits:
            print("❌ 没有找到提交记录")
            return None, None
        
        self.display_commits(commits)
        
        print("请选择提交范围 (输入序号或直接输入 commit hash)")
        print("提示: 范围是从旧到新，例如选择 1-5 表示从第5个提交到第1个提交的改动")
        print()
        
        # 选择起始提交
        start_input = self.get_user_input(
            "起始提交 (较旧)", 
            str(len(commits))
        )
        
        # 选择结束提交
        end_input = self.get_user_input(
            "结束提交 (较新)", 
            "1"
        )
        
        # 解析输入
        def parse_commit(user_input, commits_list):
            if user_input.isdigit():
                idx = int(user_input) - 1
                if 0 <= idx < len(commits_list):
                    return commits_list[idx]['hash']
            return user_input
        
        start_commit = parse_commit(start_input, commits)
        end_commit = parse_commit(end_input, commits)
        
        print(f"\n✓ 已选择范围: {start_commit}..{end_commit}")
        
        return start_commit, end_commit
    
    def set_excluded_dirs(self):
        """设置要排除的目录"""
        print("\n" + "=" * 80)
        print("配置排除目录 (可选)")
        print("=" * 80)
        print("输入要排除的目录，用逗号分隔，留空则不排除任何目录")
        print("例如: node_modules, dist, build, .vscode")
        print()
        
        excluded = self.get_user_input("排除的目录", "")
        
        if excluded:
            self.excluded_dirs = [d.strip() for d in excluded.split(',')]
            print(f"✓ 将排除以下目录: {', '.join(self.excluded_dirs)}")
        else:
            print("✓ 不排除任何目录")
    
    def generate_diff(self, start_commit, end_commit):
        """生成 diff 内容"""
        # 构建 git diff 命令
        cmd = ["git", "diff", f"{start_commit}..{end_commit}"]
        
        # 添加排除路径
        for excluded_dir in self.excluded_dirs:
            cmd.append(f":(exclude){excluded_dir}")
            cmd.append(f":(exclude){excluded_dir}/*")
        
        result = subprocess.run(
            cmd,
            cwd=self.repo_path,
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print(f"❌ 生成 diff 失败: {result.stderr}")
            return None
        
        return result.stdout
    
    def get_commit_summary(self, start_commit, end_commit):
        """获取提交范围的摘要信息"""
        cmd = [
            "git", "log",
            f"{start_commit}..{end_commit}",
            "--pretty=format:- [%h] %s (%an, %ar)",
            "--reverse"
        ]
        
        result = subprocess.run(
            cmd,
            cwd=self.repo_path,
            capture_output=True,
            text=True
        )
        
        return result.stdout
    
    def format_for_ai(self, diff_content, start_commit, end_commit):
        """格式化为适合 AI 阅读的内容"""
        summary = self.get_commit_summary(start_commit, end_commit)
        
        output = []
        output.append("# Git Diff 分析文档")
        output.append(f"\n生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        output.append(f"提交范围: {start_commit}..{end_commit}")
        
        if self.excluded_dirs:
            output.append(f"排除目录: {', '.join(self.excluded_dirs)}")
        
        output.append("\n## 提交历史\n")
        output.append(summary)
        
        output.append("\n## 代码变更详情\n")
        output.append("```diff")
        output.append(diff_content)
        output.append("```")
        
        output.append("\n## 说明")
        output.append("- '+' 开头的行表示新增的代码")
        output.append("- '-' 开头的行表示删除的代码")
        output.append("- 文件路径前有 'diff --git' 标记")
        
        return '\n'.join(output)
    
    def save_to_file(self, content, filename=None):
        """保存到文件"""
        if not filename:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"git_diff_{timestamp}.md"
        
        filepath = self.repo_path / filename
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"\n✓ Diff 文件已保存: {filepath}")
        print(f"  文件大小: {len(content)} 字符")
        
        return filepath
    
    def run(self):
        """运行主流程"""
        print("\n🚀 Git Diff 导出工具\n")
        
        # 检查 Git 仓库
        if not self.check_git_repo():
            return
        
        # 选择提交范围
        start_commit, end_commit = self.select_commit_range()
        if not start_commit or not end_commit:
            return
        
        # 设置排除目录
        self.set_excluded_dirs()
        
        # 生成 diff
        print("\n⏳ 正在生成 diff...")
        diff_content = self.generate_diff(start_commit, end_commit)
        
        if not diff_content:
            return
        
        if not diff_content.strip():
            print("⚠️  警告: 生成的 diff 为空，可能没有代码变更")
            return
        
        # 格式化输出
        formatted_content = self.format_for_ai(
            diff_content, 
            start_commit, 
            end_commit
        )
        
        # 保存文件
        output_file = self.get_user_input(
            "\n输出文件名",
            f"git_diff_{datetime.now().strftime('%Y%m%d_%H%M%S')}.md"
        )
        
        self.save_to_file(formatted_content, output_file)
        
        print("\n✅ 完成!")


def main():
    """主函数"""
    # 支持命令行参数指定仓库路径
    repo_path = sys.argv[1] if len(sys.argv) > 1 else "."
    
    exporter = GitDiffExporter(repo_path)
    
    try:
        exporter.run()
    except KeyboardInterrupt:
        print("\n\n⚠️  操作已取消")
        sys.exit(0)
    except Exception as e:
        print(f"\n❌ 发生错误: {str(e)}")
        sys.exit(1)


if __name__ == "__main__":
    main()